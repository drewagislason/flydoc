/**************************************************************************************************
  flydocprint.c - The "view" of model/view/controller for flydoc
  Copyright 2024 Drew Gislason  
  License MIT <https://mit-license.org>
**************************************************************************************************/
#include "flydoc.h"
#include "FlyStr.h"
#include "FlyList.h"
#include "FlySignal.h"
#include <stdarg.h>

const char szWarningNoModule[]     = "W001 - no module or class defined";
const char szWarningDuplicate[]    = "W002 - duplicate class, module, markdown document or mainpage: ";
const char szWarningNoFunction[]   = "W003 - function does not follow comment";
const char szWarningBadDocStr[]    = "W004 - function does preceed doc string";
const char szWarningSyntax[]       = "W005 - invalid syntax. Try flydoc --user-guide";
const char szWarningEmpty[]        = "W006 - empty content in example: indent by 4 spaces";
const char szWarningInvalidInput[] = "W007 - file or folder doesn't exist: ";
const char szWarningCreateFolder[] = "W009 - couldn't create folder: ";
const char szWarningCreateFile[]   = "W010 - couldn't create file: ";
const char szWarningNoObjects[]    = "W011 - no objects or documents defined. Nothing to do";
const char szWarningNoImage[]      = "W012 - image file not found: ";
const char szWarningReadFile[]     = "W014 - could not read possibly empty file: ";

const char szWarningMem[]          = "Warning: W013 - internal error, out of memory";

const char m_szEmpty[]             = "";

/*!
  @defgroup flydoc_print   The "view" of model/view/controller for flydoc

  All printing to the screen goes through this module.  
*/

/*!------------------------------------------------------------------------------------------------
  Print warning

  Warning: W010 - couldn't create folder: ../foo/

  @param    pDoc        ptr to document object
  @param    szWarning   ptr to warning msg
  @param    szExtra     more information, such as a folder or filename
  @return   none
-------------------------------------------------------------------------------------------------*/
void FlyDocPrintWarning(flyDoc_t *pDoc, const char *szWarning, const char *szExtra)
{
  fprintf(stderr, "Warning: %s%s\n", szWarning, szExtra ? szExtra : "");
  ++pDoc->nWarnings;
}

/*!------------------------------------------------------------------------------------------------
  Used if there is a problem with an input text file (source or markdown).

  @example  Warning With Input Text Context

      ~/folder/somefile.md:99:27: W012 - image file not found: flydoc.png
      A line with an image link ![alt text](flydoc.png "w3-circle")
                                ^

  @param    pDoc        for pDoc->nWarnings and pDoc->szPath
  @param    szWarning   ptr to warning msg
  @param    szExtra     ptr to extra information or or NULL if none
  @param    szFile      file with the problem
  @param    szFilePos   position in the file with the problem
  @return   none
-------------------------------------------------------------------------------------------------*/
void FlyDocPrintWarningEx(
  flyDoc_t     *pDoc, 
  const char   *szWarning,
  const char   *szExtra,
  const char   *szFile,
  const char   *szFilePos)
{
  unsigned    line    = 1;
  unsigned    col     = 1;
  const char *szLine  = szFile;

  line = FlyStrLinePos(szFile, szFilePos, &col);
  if(line)
    szLine = FlyStrLineGoto(szFile, line);
  fprintf(stderr, "%s:%u:%u: %s%s\n", pDoc->szPath, line, col, szWarning,
                  szExtra ? szExtra : "");
  fprintf(stderr, "%.*s\n", (unsigned)FlyStrLineLen(szLine), szLine);
  fprintf(stderr, "%*s^\n", col - 1, " ");
  ++pDoc->nWarnings;
}

/*!------------------------------------------------------------------------------------------------
  Print assert "Warning: W013 - internal error, out of memory" with stack trace.
  @return   none
-------------------------------------------------------------------------------------------------*/
void FlyDocAssertMem(void)
{
  FlySigStackTrace();
  FlyAssertFail(szWarningMem);
}

/*!------------------------------------------------------------------------------------------------
  Print this function object

  @param    pFunc     ptr to function object (not NULL)
  @param    debug     e.g. FLYDOC_DEBUG_SOME
  @param    indent    indent
  @return   none
-------------------------------------------------------------------------------------------------*/
void FlyDocPrintFunc(const flyDocFunc_t *pFunc, flyDocDbg_t debug, unsigned indent)
{
  const char  *psz;

  FlyAssert(pFunc != NULL);

   // 1 line unless DEBUG_MORE+
  if(debug <= FLYDOC_DEBUG_SOME)
  {
    printf("%*s%p: %s, szBrief: %p, szPrototype %p, szText %p\n", indent, m_szEmpty, pFunc,
           FlyStrNullOk(pFunc->szFunc), pFunc->szBrief, pFunc->szPrototype, pFunc->szText);
  }

  // >= FLYDOC_DEBUG_MORE
  else
  {
    printf("%*sflyDocFunc_t: %p\n", indent, m_szEmpty, pFunc);
    if(pFunc)
    {
      printf("%*sszFunc      = %s\n", indent + 2, m_szEmpty, FlyStrNullOk(pFunc->szFunc));
      printf("%*sszBrief     = %s\n", indent + 2, m_szEmpty, FlyStrNullOk(pFunc->szBrief));
      psz = FlyStrNullOk(pFunc->szPrototype);
      printf("%*sszPrototype = %.*s\n", indent + 2, m_szEmpty, (unsigned)FlyStrLineLen(psz), psz);
      if(pFunc->szPrototype && debug >= FLYDOC_DEBUG_MAX)
        FlyStrDump(pFunc->szPrototype, strlen(pFunc->szPrototype) + 1);
      psz = FlyStrNullOk(pFunc->szText);
      printf("%*sszText      = %.*s\n", indent + 2, m_szEmpty, (unsigned)FlyStrLineLen(psz), psz);
      if(pFunc->szText && debug >= FLYDOC_DEBUG_MAX)
        FlyStrDump(pFunc->szText, strlen(pFunc->szText) + 1);
    }
  }
}

/*!------------------------------------------------------------------------------------------------
  Print the example list

  @param    pExampleList    ptr to an example list
  @return   none
-------------------------------------------------------------------------------------------------*/
static void FlyDocPrintExampleList(const flyDocExample_t *pExampleList)
{
  const flyDocExample_t *pExample = pExampleList;

  while(pExample)
  {
    printf("    %s\n", FlyStrNullOk(pExampleList->szTitle));
    pExample = pExample->pNext;
  }
}

/*!------------------------------------------------------------------------------------------------
  Print the section, used by both FlyDocModulePrint() and FlyDocMainPagePrint().

  @param    pMainPage   ptr to main page (not NULL)
  @param    debug           e.g. FLYDOC_DEBUG_SOME
  @return   none
-------------------------------------------------------------------------------------------------*/
static void FlyDocPrintSection(const flyDocSection_t *pSection, flyDocDbg_t debug)
{
  FlyAssert(pSection);

  printf("  szTitle       = %s\n", FlyStrNullOk(pSection->szTitle));
  printf("  szSubtitle    = %s\n", FlyStrNullOk(pSection->szSubtitle));
  printf("  szBarColor    = %s\n", FlyStrNullOk(pSection->szBarColor));
  printf("  szTitleColor  = %s\n", FlyStrNullOk(pSection->szTitleColor));
  printf("  szTitleColor  = %s\n", FlyStrNullOk(pSection->szTitleColor));
  printf("  szFontBody    = %s\n", FlyStrNullOk(pSection->szFontBody));
  printf("  szFontHeadings= %s\n", FlyStrNullOk(pSection->szFontHeadings));
  printf("  szLogo        = %s\n", FlyStrNullOk(pSection->szLogo));;
  printf("  szVersion     = %s\n", FlyStrNullOk(pSection->szVersion));
  printf("  szText        = %.*s\n", pSection->szText ? (int)FlyStrLineLen(pSection->szText) : 6,
                                     FlyStrNullOk(pSection->szText));
  printf("  pExampleList = %p\n", pSection->pExampleList);
  if((debug >= FLYDOC_DEBUG_MAX) && pSection->szText)
  {
    FlyStrDump(pSection->szText, strlen(pSection->szText) + 1);
    printf("  pExampleList = %p\n", pSection->pExampleList);
  }
  if((debug >= FLYDOC_DEBUG_MORE) && pSection->pExampleList)
    FlyDocPrintExampleList(pSection->pExampleList);
}

/*!------------------------------------------------------------------------------------------------
  Print this module/class object.

  @param    pMod      ptr to module object (not NULL)
  @param    debug     e.g. FLYDOC_DEBUG_SOME
  @return   none
-------------------------------------------------------------------------------------------------*/
void FlyDocPrintModule(const flyDocModule_t *pMod, flyDocDbg_t debug)
{
  flyDocFunc_t      *pFunc;

  FlyAssert(pMod);

  // 1 line unless DEBUG_MORE+
  if(debug <= FLYDOC_DEBUG_SOME)
  {
    printf("%p: %s, nFuncs: %zu, nExamples: %zu\n", pMod, pMod->section.szTitle ? pMod->section.szTitle : "",
           FlyListLen(pMod->pFuncList), FlyListLen(pMod->section.pExampleList));
  }

  else
  {
    printf("\n-- Obj flyDocModule_t: %p: %s --\n", pMod, FlyStrNullOk(pMod->section.szTitle));
    pFunc = pMod->pFuncList;
    FlyDocPrintSection(&pMod->section, debug);
    printf("  pFuncList    = %p, items: %zu\n", pMod->pFuncList, FlyListLen(pMod->pFuncList));
    while(pFunc)
    {
      FlyDocPrintFunc(pFunc, debug, 4);
      pFunc = pFunc->pNext;
    }
  }
}

/*-------------------------------------------------------------------------------------------------
  Print the list of markdown documents

  @param    pMarkdownList   ptr to markdown document list
  @param    debug           e.g. FLYDOC_DEBUG_SOME
  @return   none
-------------------------------------------------------------------------------------------------*/
static void FlyDocPrintMarkdown(const flyDocMarkdown_t *pMarkdown, flyDocDbg_t debug)
{
  const flyDocMdHdr_t  *pMdHdr;

  printf("-- Obj flyDocMarkdown_t  %p: szTitle=%s, headings: %zu --\n", pMarkdown,
          FlyStrNullOk(pMarkdown->section.szTitle), FlyListLen(pMarkdown->pHdrList));
  FlyDocPrintSection(&pMarkdown->section, debug);
  if(debug >= FLYDOC_DEBUG_MAX)
  {
    pMdHdr = pMarkdown->pHdrList;
    while(pMdHdr)
    {
      printf("    hdr: %s\n", pMdHdr->szTitle);
      pMdHdr = pMdHdr->pNext;
    }
  }
}

/*-------------------------------------------------------------------------------------------------
  Print the list of images found in markdown / flydoc comments

  @param    pImageList   ptr to markdown document list
  @param    debug        e.g. FLYDOC_DEBUG_SOME
  @return   none
-------------------------------------------------------------------------------------------------*/
static void FlyDocPrintImageList(const flyDocImage_t *pImageList, flyDocDbg_t debug)
{
  const flyDocImage_t *pImage = pImageList;

  printf("pImageList: %p, items: %zu\n", pImageList, FlyListLen(pImageList));
  while(pImage)
  {
    printf("  szLink:  %s\n", pImage->szLink);
    pImage = pImage->pNext;
  }
}

/*-------------------------------------------------------------------------------------------------
  Print list of image files

  @param    pImgFileList    list of images files
  @param    debug           e.g. FLYDOC_DEBUG_SOME
  @return   none
-------------------------------------------------------------------------------------------------*/
static void FlyDocPrintImageFileList(const flyDocFile_t *pImgFileList, flyDocDbg_t debug)
{
  const flyDocFile_t *pImgFile = pImgFileList;

  printf("pImgFileList: %p, num files: %u\n", pImgFileList, (unsigned)FlyListLen(pImgFileList));
  while(pImgFile)
  {
    printf("  %s, %s\n", pImgFile->szPath, pImgFile->fReferenced ? "referenced" : "");
    pImgFile = pImgFile->pNext;
  }
}

/*!------------------------------------------------------------------------------------------------
  Print the main page

  @param    pMainPage   ptr to main page (not NULL)
  @param    debug           e.g. FLYDOC_DEBUG_SOME
  @return   none
-------------------------------------------------------------------------------------------------*/
void FlyDocPrintMainPage(const flyDocMainPage_t *pMainPage, flyDocDbg_t debug)
{
  FlyAssert(pMainPage);

  if(debug == FLYDOC_DEBUG_SOME)
    printf("%p: %s\n", pMainPage, FlyStrNullOk(pMainPage->section.szTitle));
  else if(debug)
  {
    printf("-- Obj MainPage: %p --\n", pMainPage);
    if(pMainPage)
      FlyDocPrintSection(&pMainPage->section, debug);
  }
}

/*-------------------------------------------------------------------------------------------------
  Helper for displaying statistics in verbose mode
-------------------------------------------------------------------------------------------------*/
static unsigned FlyDocStatWidth(const flyDoc_t *pDoc)
{
  unsigned  aStats[10];
  unsigned  width = 0;
  unsigned  thisWidth;
  unsigned  i;
  char      szStat[24]; // 64-bits = 18446744073709551615 = 20 chars

  aStats[0] = pDoc->nModules;
  aStats[1] = pDoc->nFunctions;
  aStats[2] = pDoc->nClasses;
  aStats[3] = pDoc->nMethods;
  aStats[4] = pDoc->nExamples;
  aStats[5] = pDoc->nDocuments;
  aStats[6] = pDoc->nImages;
  aStats[7] = pDoc->nFiles;
  aStats[8] = pDoc->nDocComments;
  aStats[9] = pDoc->nWarnings;

  for(i = 0; i < NumElements(aStats); ++i)
  {
    memset(szStat, 0, sizeof(szStat));
    thisWidth = snprintf(szStat, sizeof(szStat) - 1, "%u", aStats[i]);
    if(thisWidth > width)
      width = thisWidth;
  }

  return width;
}

/*!------------------------------------------------------------------------------------------------
  helper for statistics. Prints column aligned number followed by a name
-------------------------------------------------------------------------------------------------*/
void FlyDocPrintNumName(unsigned n, const char *szItem, const char *szPlural, unsigned width)
{
  printf("  %*u %s%s\n", width, n, szItem, n == 1 ? "" : szPlural);
}

/*!------------------------------------------------------------------------------------------------
  Print all the structures

  @param    pDoc    ptr to the full document structure
  @param    fFull   if FALSE, prints pointers and single lines only
  @return   none
-------------------------------------------------------------------------------------------------*/
void FlyDocPrintStats(const flyDoc_t *pDoc)
{
  unsigned  width;

  // print statistics
  width = FlyDocStatWidth(pDoc);
  printf("\n");
  FlyDocPrintNumName(pDoc->nModules,   "module",   "s",  width);
  FlyDocPrintNumName(pDoc->nFunctions, "function", "s",  width);
  FlyDocPrintNumName(pDoc->nClasses,   "class",    "es", width);
  FlyDocPrintNumName(pDoc->nMethods,   "method",   "s",  width);
  FlyDocPrintNumName(pDoc->nExamples,  "example",  "s",  width);
  FlyDocPrintNumName(pDoc->nDocuments, "document", "s",  width);
  FlyDocPrintNumName(pDoc->nImages,    "image",    "s",  width);
  printf("\n");
  printf("  %*u file%s processed\n", width, pDoc->nFiles, (pDoc->nFiles == 1) ? "" : "s");
  printf("  %*u flydoc comment%s processed\n", width, pDoc->nDocComments, (pDoc->nDocComments == 1) ? "" : "s");
  printf("  %*u warning%s\n", width, pDoc->nWarnings, (pDoc->nWarnings == 1) ? "" : "s");
}

/*!------------------------------------------------------------------------------------------------
  Print banner with centered text

  @param    szText    Some text to print
  @return   none
-------------------------------------------------------------------------------------------------*/
void FlyDocPrintBanner(const char *szText)
{
  const char szLine[] = "----------------------------------------------------------------------\n";
  unsigned   len;

  // center the 
  len = strlen(szText);
  if(len >= strlen(szLine) - 1)
    len = 0;
  else
    len = (len - strlen(szLine)) / 2;

  printf("%s%*s%s\n%s\n", szLine, len, "", szText, szLine);
}

/*!------------------------------------------------------------------------------------------------
  Print all the structures

  @param    pDoc    ptr to the document structure
  @param    level   verbosity: FLYDOC_PRINT_MIN, FLYDOC_PRINT_MORE, FLYDOC_PRINT_MAX
  @return   none
-------------------------------------------------------------------------------------------------*/
void FlyDocPrintDoc(const flyDoc_t *pDoc, flyDocDbg_t debug)
{
  static const char szFmt[] = "FlyDocPrintDoc %p, debug %u";
  char              szText[sizeof(szFmt) + sizeof(pDoc) + 1];
  flyDocModule_t   *pMod;
  flyDocMarkdown_t *pMarkdown;

  snprintf(szText, sizeof(szText), szFmt, pDoc, debug);
  FlyDocPrintBanner(szText);
  if(pDoc->pMainPage == NULL)
    printf("  pMainPage %p\n", pDoc->pMainPage);
  else
    FlyDocPrintMainPage(pDoc->pMainPage, debug);
  printf("  pModList      %p, %u module(s)\n", pDoc->pModList, (unsigned)FlyListLen(pDoc->pModList));
  pMod = pDoc->pModList;
  while(pMod)
  {
    FlyDocPrintModule(pMod, debug);
    pMod = pMod->pNext;
  }
  printf("  pClassList    %p, %u class(es)\n", pDoc->pClassList, (unsigned)FlyListLen(pDoc->pClassList));
  pMod = pDoc->pClassList;
  while(pMod)
  {
    FlyDocPrintModule(pMod, debug);
    pMod = pMod->pNext;
  }
  printf("  pMarkdownList %p, %u document(s)\n", pDoc->pMarkdownList, (unsigned)FlyListLen(pDoc->pMarkdownList));
  pMarkdown = pDoc->pMarkdownList;
  while(pMarkdown)
  {
    FlyDocPrintMarkdown(pMarkdown, debug);
    pMarkdown = pMarkdown->pNext;
  }

  // print image lists
  FlyDocPrintImageList(pDoc->pImageList, debug);
  FlyDocPrintImageFileList(pDoc->pImgFileList, debug);

  FlyDocPrintStats(pDoc);
}

/*!------------------------------------------------------------------------------------------------
  Ask if user wants to overwrite file

  @param    szFilename    name of file to check if user wants to overwrite
  @param    fFull   if FALSE, prints pointers and single lines only
  @return   none
-------------------------------------------------------------------------------------------------*/
bool_t FlyDocCheckOverWrite(const char *szFilename)
{
  static const char szFmt[] = "Are you sure you want to overwrite file %s?";
  flyStrSmart_t    *pQuestion;
  char              szAnswer[8];
  bool_t            fOverwrite = FALSE;

  pQuestion = FlyStrSmartAlloc(sizeof(szFmt) + strlen(szFilename));
  if(pQuestion)
  {
    FlyStrSmartSprintf(pQuestion, szFmt, szFilename);
    FlyStrAsk(szAnswer, pQuestion->sz, sizeof(szAnswer));
    if(toupper(*szAnswer) == 'Y')
      fOverwrite = TRUE;
    FlyStrSmartFree(pQuestion);
  }
  return fOverwrite;
}

/*!------------------------------------------------------------------------------------------------
  Print for debugging

  @param    szFormat    constant format string
  @param    ...         optional variable number of parameters
  @return   length of printed string
*///-----------------------------------------------------------------------------------------------
int FlyDbgPrintf(const char *szFormat, ...)
{
  va_list     arglist;
  int         len;

  // print, and clear to end of line
  va_start(arglist, szFormat);
  len = vprintf(szFormat, arglist);
  va_end(arglist);

  return len;
}

/*!------------------------------------------------------------------------------------------------
  Print a slug given a string

  @param    szTitle     a string to convert to a referencer
  @return  length of printed string
*///-----------------------------------------------------------------------------------------------
void FlyDocPrintSlug(const char *szTitle)
{
  size_t    len;
  char     *szRef;

  len = FlyDocStrToRef(NULL, UINT_MAX, NULL, szTitle);
  szRef = FlyAllocZ(len + 1);
  if(szRef)
  {
    FlyDocStrToRef(szRef, len + 1, NULL, szTitle);
    printf("%s\n", szRef);
    FlyFree(szRef);
  }
}
