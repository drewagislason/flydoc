/**************************************************************************************************
  flydocmd.c - markdown output for flydoc
  Copyright 2024 Drew Gislason
  License MIT <https://mit-license.org>
**************************************************************************************************/
#include "flydoc.h"
#include "FlyFile.h"
#include "FlyList.h"
#include "FlyStr.h"
#include "FlyMarkdown.h"

/*!
  @defgroup   flydoc_md   Create markdown output from flyDoc_t structure
*/

/*!------------------------------------------------------------------------------------------------
  Write the text portion of a module/class or function. Converts @example properly.

  @param  fpOut   output file pointer
  @param  szText  text to write
  @return none
-------------------------------------------------------------------------------------------------*/
void FlyDocWriteMarkdownText(FILE *fpOut, const char *szText)
{
  const char       *szTitle;
  const char       *szLine;
  flyDocKeyword_t   keyword;

  szLine = szText;
  while(szLine && *szLine)
  {
    if(FlyDocIsKeyword(szLine, &keyword) && keyword == FLYDOC_KEYWORD_EXAMPLE)
    {
      szTitle = FlyStrArgNext(FlyStrSkipWhite(szLine));
      fprintf(fpOut, "**Example: %.*s**\n", (int)FlyStrLineLen(szTitle), szTitle);
    }
    else
      fprintf(fpOut, "%.*s\n", (int)FlyStrLineLen(szLine), szLine);
    szLine = FlyStrLineNext(szLine);
  }
}

/*!------------------------------------------------------------------------------------------------
  Write the pDoc data to a markdown file

  @param  pDoc    flydoc state with open file ptr pDoc->fpOut
  @param  pList   text to write
  @param  szPre   prefix
  @param  level   level to add to any headers
  @return none  
-------------------------------------------------------------------------------------------------*/
void FlyDocWriteMarkdownModList(flyDoc_t *pDoc, flyDocModule_t *pList, const char *szPre, unsigned level)
{
  const char        szHash[] = "########";
  flyDocModule_t   *pMod;
  flyDocFunc_t     *pFunc;

  FlyAssert(pDoc && pDoc->fpOut);

  if(pDoc->opts.debug)
    printf("--- FlyDocWriteMarkdownModList(pList %p, level %u) ---\n", pList, level);
  if(pDoc->opts.fNoBuild)
    return;

  ++level;
  pMod = pList;
  while(pMod)
  {
    if(pDoc->opts.debug >= FLYDOC_DEBUG_MAX)
      printf("pMod %p, title: %s\n", pMod, FlyStrNullOk(pMod->section.szTitle));
    FlyAssert(pMod->section.szTitle);

    // output title of module and module text
    fprintf(pDoc->fpOut, "%.*s %s%s\n\n", level, szHash, szPre, pMod->section.szTitle);
    if(pMod->section.szSubtitle)
      fprintf(pDoc->fpOut, "%s\n\n", pMod->section.szSubtitle);
    if(pMod->section.szText)
    {
      FlyDocWriteMarkdownText(pDoc->fpOut, pMod->section.szText);
      fprintf(pDoc->fpOut, "\n");
    }

    if(pMod->pFuncList)
    {
      ++level;
      pFunc = pMod->pFuncList;

      while(pFunc)
      {
        if(pDoc->opts.debug >= FLYDOC_DEBUG_MORE)
          printf("pFunc %p, szFunc: %s\n", pFunc, FlyStrNullOk(pFunc->szFunc));
        FlyAssert(pFunc->szFunc);
        fprintf(pDoc->fpOut, "%.*s %s\n\n", level, szHash, pFunc->szFunc);

        ++level;
        if(pFunc->szBrief)
          fprintf(pDoc->fpOut, "%s\n\n", pFunc->szBrief);
        if(pFunc->szPrototype)
        {
          fprintf(pDoc->fpOut, "%.*s Prototype\n\n", level, szHash);
          fprintf(pDoc->fpOut, "```%s\n", pFunc->szLang ? pFunc->szLang : "");
          fprintf(pDoc->fpOut, "\n%s```\n\n", pFunc->szPrototype);
        }
        if(pFunc->szText)
        {
          fprintf(pDoc->fpOut, "%.*s Notes\n\n", level, szHash);
          FlyDocWriteMarkdownText(pDoc->fpOut, pFunc->szText);
          fprintf(pDoc->fpOut, "\n");
        }
        --level;

        pFunc = pFunc->pNext;
      }
      --level;
    }

    pMod = pMod->pNext;
  }
}

/*!------------------------------------------------------------------------------------------------
  Write the markdown list to the markdown file. 

  Basically, as-is exept headers which may increase level.

  @param  pDoc    flydoc state with open file ptr pDoc->fpOut
  @param  pList   text to write
  @param  level   level to add to any headers
  @return none    
-------------------------------------------------------------------------------------------------*/
void FlyDocWriteMarkdownList(flyDoc_t *pDoc, flyDocMarkdown_t *pMarkdownList, unsigned level)
{
  flyDocMarkdown_t *pMarkdown;
  const char       *szLine;
  const char       *szLineBeg;
  char             *szNext;
  char              szHdr[8]; // allows up to level 6 headers 
  unsigned          thisLevel;
  size_t            len;

  FlyAssert(pDoc && pDoc->fpOut);

  if(pDoc->opts.debug)
    printf("--- FlyDocWriteMarkdownList(pMarkdownList %p, level %u) ---\n", pMarkdownList, level);
  if(pDoc->opts.fNoBuild)
    return;

  pMarkdown = pMarkdownList;
  while(pMarkdown)
  {
    // if not adding level to headers, just write the markdown file as-is
    if(level == 0)
      fputs(pMarkdown->szFile, pDoc->fpOut);
    else
    {
      // add level to each header
      szLine = szLineBeg = pMarkdown->szFile;
      while(szLine && *szLine)
      {
        if(FlyMd2HtmlIsHeading(szLine, &thisLevel))
        {
          // write all lines up to the header
          if((szLine - szLineBeg) > 0)
            fwrite(szLineBeg, 1, (szLine - szLineBeg), pDoc->fpOut);

          // write the header with potentially extra levels
          thisLevel += level;
          if(thisLevel > 6)
            thisLevel = 6;
          FlyStrZFill(szHdr, '#', sizeof(szHdr), thisLevel);
          FlyStrZCat(szHdr, " ", sizeof(szHdr));
          fwrite(szHdr, 1, strlen(szHdr), pDoc->fpOut);

          szLine = FlyStrSkipChars(szLine, "#");
          szNext = FlyStrLineNext(szLine);
          fwrite(szLine, 1, (unsigned)(szNext - szLine), pDoc->fpOut);

          // on to next line after header
          szLine = szLineBeg = szNext;
          continue;
        }

        szLine = FlyStrLineNext(szLine);
      }

      // write last bit, if any
      if((szLine - szLineBeg) > 0)
        fwrite(szLineBeg, 1, (szLine - szLineBeg), pDoc->fpOut);
    }

    // if last line is not blank, add a line
    if(pMarkdown->pNext)
    {
      len = strlen(pMarkdown->szFile);
      if(len > 2 && (pMarkdown->szFile[len - 1] != '\n' || pMarkdown->szFile[len - 2] != '\n'))
        fputs("\n", pDoc->fpOut);
    }

    // on to next markdown file
    pMarkdown = pMarkdown->pNext;
  }
}

/*!------------------------------------------------------------------------------------------------
  Write the pDoc to a single markdown file (everything)

  @param  pDoc    flydoc state with open file ptr pDoc->fpOut
  @return none
-------------------------------------------------------------------------------------------------*/
bool_t FlyDocWriteMarkdown(flyDoc_t *pDoc)
{
  static const char szMdExt[] = ".md";
  flyDocMainPage_t *pMainPage = pDoc->pMainPage;
  unsigned          level = 0;
  unsigned          countMods;
  unsigned          len;
  unsigned          size;
  char             *szNameLast;
  char             *szOutFile;
  char             *szFullPath;

  if(pDoc->opts.debug)
    printf("-- FlyDocWriteMarkdown(%s) ---\n", pDoc->opts.szOut);

  // create the folder
  if(!FlyDocCreateFolder(pDoc, pDoc->opts.szOut))
  {
    FlyDocPrintWarning(pDoc, szWarningCreateFolder, pDoc->opts.szOut);
    return FALSE;
  }

  // create filename
  szFullPath = FlyDocAlloc(PATH_MAX);
  FlyFileFullPath(szFullPath, pDoc->opts.szOut);
  szNameLast = FlyStrPathNameLast(szFullPath, &len);
  memmove(szFullPath, szNameLast, len);
  szFullPath[len] = '\0';
  szNameLast = szFullPath;
  size = strlen(pDoc->opts.szOut) + len + sizeof(szMdExt) + 4;  // enough room to concatinate paths
  szOutFile = FlyDocAlloc(size);
  FlyStrZCpy(szOutFile, pDoc->opts.szOut, size);
  FlyStrPathAppend(szOutFile, szNameLast, size);
  FlyStrZCat(szOutFile, szMdExt, size);
  printf("  %s\n", szOutFile);

  // open the file to write
  pDoc->fpOut = fopen(szOutFile, "w");
  if(!pDoc->fpOut)
  {
    FlyDocPrintWarning(pDoc, szWarningCreateFile, szOutFile);
    return FALSE;
  }

  if(pDoc->opts.debug >= FLYDOC_DEBUG_MORE)
  {
    printf("  pMainPage %p, pModList %p (%zu), pClassList %p (%zu), pMarkdownList %p (%zu))\n", 
        pDoc->pMainPage, pDoc->pModList, FlyListLen(pDoc->pModList),
        pDoc->pClassList, FlyListLen(pDoc->pClassList),
        pDoc->pMarkdownList, FlyListLen(pDoc->pMarkdownList));
  }

  // detemine # of modules/classes/documents (this determies starting level, number of ##)
  countMods = pDoc->nModules + pDoc->nClasses + pDoc->nDocuments;

  // mainpage is present, that is the main header
  if(pMainPage)
  {
    fprintf(pDoc->fpOut, "# %s\n\n", pMainPage->section.szTitle);
    if(pMainPage->section.szSubtitle)
      fprintf(pDoc->fpOut, "%s\n\n", pMainPage->section.szSubtitle);
    if(pMainPage->section.szVersion)
      fprintf(pDoc->fpOut, "version %s\n\n", pMainPage->section.szVersion);
    if(pMainPage->section.szText)
    {
      fputs(pMainPage->section.szText, pDoc->fpOut);
      if(!FlyCharIsEol(FlyStrCharLast(pMainPage->section.szText)))
        fprintf(pDoc->fpOut, "\n");
      fprintf(pDoc->fpOut, "\n");
    }
    ++level;
  }

  // zero or multiple (>1) modules/classes without mainpage, so this is the main page
  if(pMainPage == NULL && countMods != 1)
  {
    fprintf(pDoc->fpOut, "# Project %s\n\n", szNameLast);
    fprintf(pDoc->fpOut, "%u Modules\n", pDoc->nModules);
    fprintf(pDoc->fpOut, "%u Classes\n", pDoc->nClasses);
    fprintf(pDoc->fpOut, "%u Markdown Documents\n", pDoc->nDocuments);
    fprintf(pDoc->fpOut, "%u Examples\n\n", pDoc->nExamples);
    ++level;
  }

  FlyDocWriteMarkdownModList(pDoc, pDoc->pModList, "", level);
  FlyDocWriteMarkdownModList(pDoc, pDoc->pClassList, "Class ", level);
  FlyDocWriteMarkdownList(pDoc, pDoc->pMarkdownList, level);

  FlyFreeIf(szOutFile);
  FlyFreeIf(szFullPath);

  return TRUE;
}
