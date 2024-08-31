/**************************************************************************************************
  flydocparse.c - Parse input files into the flyDoc_t structure
  Copyright 2024 Drew Gislason  
  License MIT <https://mit-license.org>
**************************************************************************************************/
#include <sys/stat.h>
#include "flydoc.h"
#include "FlyCli.h"
#include "FlyFile.h"
#include "FlyList.h"
#include "FlyMarkdown.h"
#include "FlyStr.h"

#define SANCHK_DOC    987

#define PARSE_CLASS   1
#define PARSE_MODULE  0

static const char szTwoLines[]    = "\n\n";     // for ending sections
static const char szFlyDocExtra[] = "  ";       // padding on end of markdown line for break

static const char m_szImageExts[] = ".jpg.jpeg.png.gif";
static const char m_szMarkdownExts[] = ".md.mdown.markdown";

/*!
  @defgroup flydoc_parse   Parses source comments and markdown into flydoc structures.

  Prior to output, the inputs MUST be in consistent form. This will issue warnings if there are
  syntax errors, missing image files, etc..., but the whatever the inputs the result internal
  structures are always valid for output.
*/

/*!------------------------------------------------------------------------------------------------
  Does this line start with a keyword? If so, return which keyword.

  @param    szLine    ptr to a line in the comment header
  @param    pKeyword  return value if it's a keyword (which one)
  @return   ptr to argument after the keyword, or NULL if not a keyword
-------------------------------------------------------------------------------------------------*/
const char * FlyDocIsKeyword(const char *szLine, flyDocKeyword_t *pKeyword)
{
  // IMPORTANT! Adjust enum flyDocKeyword_t to match
  static const char *aszKeywords[] =
  {
    "@class",    // @class    name description
    "@color",    // @color    sidebar {titlebar}  // e.g. @color w3-red w3-blue
    "@defgroup", // @defgroup name description
    "@example",  // @example  title
    "@fn",       // @fn       prototype
    "@font",     // @font     body headings       // e.g. @font "American Typewriter" Garamond
    "@inclass",  // @ingroup  name
    "@ingroup",  // @ingroup  name
    "@logo",     // @logo link
    "@mainpage", // @mainpage title description
    "@param",    // @param    name description    // psuedo keyword, just included in text
    "@return",   // @return   description         // psuedo keyword, just included in text
    "@returns",  // @returns  Same as @return     // psuedo keyword, just included in text
    "@version"   // @version  string
  };

  bool_t          fIsKeyword = FALSE;
  unsigned        i;
  unsigned        len;
  flyDocKeyword_t keyword    = FLYDOC_KEYWORD_UNKNOWN;

  // note: keywords MUST be at the left-most column
  if(*szLine == '@')
  {
    fIsKeyword  = TRUE;
    for(i = 0; i < NumElements(aszKeywords); ++i)
    {
      len = strlen(aszKeywords[i]);
      if((strncmp(szLine, aszKeywords[i], len) == 0) && isspace(szLine[len]))
      {
        keyword = (flyDocKeyword_t)i;
        break;
      }
    }

    if(pKeyword)
      *pKeyword = keyword;
  }

  // argument after @keyword or NULL
  if(fIsKeyword)
    szLine = FlyStrArgNext(szLine);
  else
    szLine = NULL;

  return szLine;  
}

/*!------------------------------------------------------------------------------------------------
  Section keywords are @class, @defgroup, @fn, @mainpage

  @param    szLine    ptr to a line in the comment header
  @param    pKeyword  return value if it's a section keyword, e.g. FLYDOC_KEYWORD_DEFGROUP
  @return   TRUE if this line starts with a section keyword
-------------------------------------------------------------------------------------------------*/
bool_t FlyDocIsSection(flyDocKeyword_t keyword)
{
  return (keyword == FLYDOC_KEYWORD_CLASS ||
          keyword == FLYDOC_KEYWORD_DEFGROUP ||
          keyword == FLYDOC_KEYWORD_FN ||
          keyword == FLYDOC_KEYWORD_MAINPAGE);
}

/*!------------------------------------------------------------------------------------------------
  The prototype keywords get moved to the function prototype, but are removed in normal parsing.

  @param    keyword   keyword
  @return   TRUE if keyword doesn't end up in szText
-------------------------------------------------------------------------------------------------*/
bool_t FlyDocIsKeywordProto(flyDocKeyword_t keyword)
{
  bool_t  fIsProto = FALSE;

  if(keyword == FLYDOC_KEYWORD_PARAM || keyword == FLYDOC_KEYWORD_RETURN ||
     keyword == FLYDOC_KEYWORD_RETURNS || keyword == FLYDOC_KEYWORD_UNKNOWN)
  {
    fIsProto = TRUE;
  }

  return fIsProto;
}

/*!------------------------------------------------------------------------------------------------
  Fixup the pointer to point to proper place in pDoc->szFile, if not already pointing there.

  @param    pDoc    document state
  @param    szPos   position in either pDoc->szFile or copy of the doc header
  @return   TRUE if keyword doesn't end up in szText
-------------------------------------------------------------------------------------------------*/
const char * FlyDocFixupPos(flyDoc_t *pDoc, const char *szPos)
{
  const char *pszFilePos = szPos;

  // if position is already in the file, we're done
  if(!(szPos >= pDoc->szFile && szPos <= pDoc->szFile + strlen(pDoc->szFile)))
  {
    if(pDoc->pCurHdr)
    {
      FlyAssert(pDoc->szCurHdr);
      pszFilePos = FlyStrHdrCpyPos(pDoc->szCurHdr, pDoc->pCurHdr, szPos);
    }
  }

  return pszFilePos;
}

/*!------------------------------------------------------------------------------------------------
  Create a base name from a file path, e.g. "../path/markdown.md" becomes "markdown"

  @param  szBaseName  dst string buffer
  @param  szTitle     src string
  @param  size        size of dst string buffer
  @return length of szNameBase
-------------------------------------------------------------------------------------------------*/
unsigned FlyDocMakeNameBase(char *szNameBase, const char *szTitle, size_t size)
{
  const char *pszName;
  unsigned    len       = 0;
   
 // remove the .md from the title (e.g. "../path/markdown.md" becomes "markdown")
  pszName = FlyStrPathNameBase(szTitle, &len);
  FlyStrZNCpy(szNameBase, pszName, size, len);

  return len;
}

/*!------------------------------------------------------------------------------------------------
  Calculate space needed for text field adding 2 spaces per line for markdown line break.

  Allows room for '\0' and 2 spaces per line, in addition to what's in each line.

  @param    szLine    ptr to a line in the comment header
  @param    szEnd     end of section to process
  @return   total size of text area
-------------------------------------------------------------------------------------------------*/
size_t FlyDocTextLenCalc(const char *szLine, const char *szEnd)
{
  size_t    size = (szEnd - szLine) + 1;
  unsigned  extra = strlen(szFlyDocExtra);

  while(*szLine && szLine < szEnd)
  {
    size += extra;
    szLine = FlyStrLineNext(szLine);
  }

  return size;
}

/*!------------------------------------------------------------------------------------------------
  Returns TRUE if this line has at least 2 spaces at end. 

  @param    szLine    ptr to a line of text
  @return   TRUE if extra spaces already at end of line
-------------------------------------------------------------------------------------------------*/
bool_t FlyDocExtraIsAtEnd(const char *szLine)
{
  const char *psz;      
  bool_t      fSpacesAtEnd = FALSE;

  if(FlyStrLineLen(szLine) >= 2)
  {
    psz = FlyStrLineEnd(szLine) - 2;
    if(psz[0] == ' ' && psz[1] == ' ')
      fSpacesAtEnd = TRUE;
  }

  return fSpacesAtEnd;
}

/*!------------------------------------------------------------------------------------------------
  Copy a line, padding with extra spaces if neeeded

  @param    szLine    ptr to a line of text
  @return   TRUE if extra spaces already at end of line
-------------------------------------------------------------------------------------------------*/
char * FlyDocExtraLineCopy(char *psz, const char *szLine)
{
  unsigned len;

  len = FlyStrLineLen(szLine);
  memcpy(psz, szLine, len);
  psz += len;
  if(!FlyDocExtraIsAtEnd(szLine))
  {
    strcpy(psz, szFlyDocExtra);
    psz += strlen(szFlyDocExtra);
  }
  if(szLine[len] == '\r')
    *psz++ = '\r';
  *psz++ = '\n';

  return psz;
}

/*!------------------------------------------------------------------------------------------------
  Add extra space to all lines that need it. Assumes asciiz string is large enough to expand.

  @param    szLine    ptr to first line
  @param    fSmart    doesn't put extra space on keyword lines
  @return   none
-------------------------------------------------------------------------------------------------*/
void FlyDocExtraAddAll(char *szLine)
{
  char       *psz;
  char       *szEnd;
  unsigned    lenExtra = strlen(szFlyDocExtra);

  szEnd = szLine + strlen(szLine) + 1;
  while(*szLine)
  {
    if(!FlyDocExtraIsAtEnd(szLine))
    {
      psz = szLine + FlyStrLineLen(szLine);
      memmove(psz + lenExtra, psz, (szEnd - psz));
      memset(psz, ' ', lenExtra);
      szEnd += lenExtra;
    }

    szLine = FlyStrLineNext(szLine);
  }
}

/*!------------------------------------------------------------------------------------------------
  Are all the lines Blank?

  @param    szLine    ptr to a line in the comment header
  @param    szEnd     end of section to process
  @return   TRUE if this all the lines are blank
-------------------------------------------------------------------------------------------------*/
bool_t FlyDocIsEmpty(const char *szLine, const char *szEnd)
{
  bool_t  fIsEmpty = TRUE;

  while(*szLine && szLine < szEnd)
  {
    if(!FlyStrLineIsBlank(szLine))
    {
      fIsEmpty = FALSE;
      break;
    }
    szLine = FlyStrLineNext(szLine);
  }

  return fIsEmpty;
}

/*!------------------------------------------------------------------------------------------------
  Get ptrs to name and description strings in pattern "@keyword name description"

  - Returns FALSE if 1st non-blank on a line doesn't start with keyword  
  - Returns FALSE if no name
  - Description is optional, that is, it may point to end of line, or may point to many words

  @param    szLine            ptr to the pattern @keyword name description
  @param    ppszName          ptr to receive name
  @param    ppszDescription   ptr to receive description
  @return   FALSE if doesn't match pattern
-------------------------------------------------------------------------------------------------*/
bool_t FlyDocGetNameDescription(const char *szLine, const char **ppszName, const char **ppszDescription)
{
  const char *psz;
  bool_t      fPatternMatched = FALSE;
  size_t      len;

  psz = FlyStrSkipWhite(szLine);
  if(*psz == '@')
  {
    psz  = FlyStrArgNext(psz);
    len  = FlyStrArgLen(psz);
    if(len)
    {
      fPatternMatched = TRUE;
      *ppszName = psz;
      *ppszDescription = FlyStrArgNext(psz);
    }
  }

  return fPatternMatched;
}

/*!------------------------------------------------------------------------------------------------
  Given a pointer to a CName, allocate it

  Example:

      snake_case_name more text on line

  would return "snake_case_name"

  @param    szCName     pointer to the CName variable
  @return   allocated C Name, or NULL if not a CName
-------------------------------------------------------------------------------------------------*/
char * FlyDocCNameAlloc(const char *sz)
{
  char       *szCName = NULL;
  unsigned    len;

  len = FlyStrCNameLen(sz);
  if(len)
  {
    szCName = FlyAlloc(len + 1);
    if(szCName)
      FlyStrZNCpy(szCName, sz, len + 1, len);
  }

  return szCName;
}

/*!------------------------------------------------------------------------------------------------
  Allocate a copy of string to end of line

  @param    sz      string
  @return   ptr to allocated string or NULL if failed
-------------------------------------------------------------------------------------------------*/
char * FlyDocAllocToLineEnd(const char *sz)
{
  char     *szNew;
  unsigned  len;

  len = FlyStrLineEnd(sz) - sz;
  szNew = FlyAlloc(len + 1);
  if(szNew)
  {
    if(len)
      strncpy(szNew, sz, len);
    szNew[len] = '\0';
  }

  return szNew;
}

/*!------------------------------------------------------------------------------------------------
  Create a new function for a module or class

  @param    pList      list of functions for this module (may be NULL if 1st)
  @param    szCName    ptr to allocated or static string
  @param    szModule   ptr to allocated or static string
  @return   TRUE if worked, FALSE if there was an error
-------------------------------------------------------------------------------------------------*/
flyDocFunc_t * FlyDocFuncFree(flyDocFunc_t *pFunc)
{
  if(pFunc)
  {
    if(pFunc->szFunc)
      FlyFree(pFunc->szFunc);
    if(pFunc->szBrief)
      FlyFree(pFunc->szBrief);
    if(pFunc->szPrototype)
      FlyFree(pFunc->szPrototype);
    if(pFunc->szText)
      FlyFree(pFunc->szText);
    memset(pFunc, 0, sizeof(*pFunc));
  }

  return NULL;
}

/*!------------------------------------------------------------------------------------------------
  Create a new function.

  szFunc can point "main(int argc..." or "main   (..." or "main"

  @param    szFunc    name of function may or may not be '\0' terminated
  @return   ptr to function or NULL
-------------------------------------------------------------------------------------------------*/
flyDocFunc_t * FlyDocFuncNew(const char *szFunc)
{
  flyDocFunc_t *pFunc = NULL;
  unsigned      len = 0;

  if(szFunc)
    len = FlyStrCNameLen(szFunc);
  if(len)
  {
    pFunc = FlyAlloc(sizeof(*pFunc));
    if(pFunc)
    {
      memset(pFunc, 0, sizeof(*pFunc));
      pFunc->szFunc  = FlyStrAllocN(szFunc, len);
      if(!pFunc->szFunc)
        pFunc = FlyDocFuncFree(pFunc);
    }
  }

  return pFunc;
}

/*!------------------------------------------------------------------------------------------------
  Add this function to the list.

  @param    pList   list of functions for this module (may be NULL if 1st)
  @param    pFunc   a function object to add to list
  @return   ptr new list
-------------------------------------------------------------------------------------------------*/
int FlyDocFuncListCmp(const void *pThis, const void *pThat)
{
  const flyDocFunc_t *pFunc1 = pThis;
  const flyDocFunc_t *pFunc2 = pThat;

  FlyAssert(pThis && pThat);
  return strcasecmp(pFunc1->szFunc, pFunc2->szFunc);
}

/*!------------------------------------------------------------------------------------------------
  Add this function to the list.

  @param    pList   list of functions for this module (may be NULL if 1st)
  @param    pFunc   a function object to add to list
  @return   ptr new list
-------------------------------------------------------------------------------------------------*/
flyDocFunc_t * FlyDocFuncListAdd(flyDocFunc_t *pList, flyDocFunc_t *pFunc, bool_t fSort)
{
  if(fSort)
    pList = FlyListAddSorted(pList, pFunc, FlyDocFuncListCmp);
  else
    pList = FlyListAppend(pList, pFunc);

  return pList;
}

/*!------------------------------------------------------------------------------------------------
  Is this in the function list?

  @param    pList      list of functions for this module (may be NULL if 1st)
  @param    pList      a string with a function name
  @return   ptr function or NULL if not found
-------------------------------------------------------------------------------------------------*/
flyDocFunc_t * FlyDocFuncInList(flyDocFunc_t *pList, const char *szFunc)
{
  flyDocFunc_t *pFunc = NULL; 

  while(pList)
  {
    if(strcmp(szFunc, pList->szFunc) == 0)
    {
      pFunc = pList;
      break;
    }
    pList = pList->pNext;
  }

  return pFunc;
}

/*!------------------------------------------------------------------------------------------------
  Free an example

  @param    pExample  ptr to example
  @return   NULL
-------------------------------------------------------------------------------------------------*/
flyDocExample_t * FlyDocExampleFree(flyDocExample_t *pExample)
{
  if(pExample)
  {
    if(pExample->szTitle)
      FlyFree(pExample->szTitle);
    memset(pExample, 0, sizeof(*pExample));
  }

  return NULL;
}

/*!------------------------------------------------------------------------------------------------
  Create a new example

  @param    szTitle   title of the example, line or '\0' terminated
  @return   ptr to example or NULL if couldn't allocat ememory
-------------------------------------------------------------------------------------------------*/
flyDocExample_t * FlyDocExampleNew(const char *szTitle)
{
  const char        szExamplePrefix[] = "Example: ";
  unsigned          size;
  unsigned          titleLen;
  flyDocExample_t  *pExample = NULL;

  titleLen = FlyStrLineLen(szTitle);
  if(szTitle)
  {
    pExample = FlyAlloc(sizeof(*pExample));
    if(pExample)
    {
      memset(pExample, 0, sizeof(*pExample));
      size = sizeof(szExamplePrefix) + titleLen;
      pExample->szTitle = FlyAlloc(size);
      if(pExample->szTitle == NULL)
        pExample = FlyDocExampleFree(pExample);
      else
      {
        FlyStrZCpy(pExample->szTitle, szExamplePrefix, size);
        FlyStrZNCat(pExample->szTitle, szTitle, size, titleLen);
        FlyStrBlankRemove(pExample->szTitle);
      }
    }
  }

  return pExample;
}

/*!------------------------------------------------------------------------------------------------
  Is an example of this title in the list?

  @param    pList     list of examples
  @param    szTitle   title of example to look for
  @return   ptr to found example or NULL if not found
-------------------------------------------------------------------------------------------------*/
flyDocExample_t * FlyDocExampleInList(flyDocExample_t *pList, const char *szTitle)
{
  flyDocExample_t *pExample = NULL;

  while(pList)
  {
    if(strcmp(pList->szTitle, szTitle) == 0)
    {
      pExample = pList;
      break;
    }
    pList = pList->pNext;
  }

  return pExample;
}

/*!------------------------------------------------------------------------------------------------
  Free the module and all it's fields

  @param    pSection    the section to free
  @return   ptr to new head of list
-------------------------------------------------------------------------------------------------*/
void FlyDocSectionFree(flyDocSection_t *pSection)
{
  if(pSection->szTitle)
    FlyFree(pSection->szTitle);
  if(pSection->szSubtitle)
    FlyFree(pSection->szSubtitle);
  if(pSection->szText)
    FlyFree(pSection->szText);
  if(pSection->szVersion)
    FlyFree(pSection->szVersion);
  if(pSection->szBarColor)
    FlyFree(pSection->szBarColor);
  if(pSection->szTitleColor)
    FlyFree(pSection->szTitleColor);
  if(pSection->szLogo)
    FlyFree(pSection->szLogo);
  memset(pSection, 0, sizeof(*pSection));
}

/*!------------------------------------------------------------------------------------------------
  Free the module and all it's fields

  @param    pMod    The module to free
  @return   ptr to new head of list
-------------------------------------------------------------------------------------------------*/
flyDocModule_t * FlyDocModFree(flyDocModule_t *pMod)
{
  if(pMod)
  {
    FlyDocSectionFree(&pMod->section);

    // TODO: free chain of functions and examples
    FlyFree(pMod);
  }

  return NULL;
}

/*!------------------------------------------------------------------------------------------------
  Create a new module

  @param    szTitle   ptr to CName title, need not be '\0' terminated
  @return   ptr to new head of list
-------------------------------------------------------------------------------------------------*/
flyDocModule_t * FlyDocModNew(const char *szTitle)
{
  flyDocModule_t *pMod = NULL;
  size_t          len;

  if(szTitle)
  {
    len = FlyStrArgLen(szTitle);
    if(len)
    {
      pMod = FlyAlloc(sizeof(*pMod));
      if(pMod)
      {
        memset(pMod, 0, sizeof(*pMod));
        pMod->section.szTitle = FlyStrAllocN(szTitle, len);
        if(!pMod->section.szTitle)
          pMod = FlyDocModFree(pMod);
      }
    }
  }

  return pMod;
}

/*!------------------------------------------------------------------------------------------------
  Add this function to the list.

  @param    pList   list of functions for this module (may be NULL if 1st)
  @param    pFunc   a function object to add to list
  @return   ptr new list
-------------------------------------------------------------------------------------------------*/
int FlyDocModListCmp(const void *pThis, const void *pThat)
{
  const flyDocModule_t *pMod1 = pThis;
  const flyDocModule_t *pMod2 = pThat;

  FlyAssert(pMod1 && pMod2);
  return strcasecmp(pMod1->section.szTitle, pMod2->section.szTitle);
}

/*!------------------------------------------------------------------------------------------------
  Add the allocated module to the list.

  @param    pList   list of modules or classes
  @return   ptr to new head of list
-------------------------------------------------------------------------------------------------*/
flyDocModule_t * FlyDocModListAdd(flyDocModule_t *pList, flyDocModule_t *pMod, bool_t fSort)
{
  if(fSort)
    pList = FlyListAddSorted(pList, pMod, FlyDocModListCmp);
  else
    pList = FlyListAppend(pList, pMod);

  return pList;
}

/*!------------------------------------------------------------------------------------------------
  Is this title found in the module or class list?

  @param    pList      list of modules or classes
  @param    szTitle    a CName title string
  @return   ptr function or NULL if not found
-------------------------------------------------------------------------------------------------*/
flyDocModule_t * FlyDocModInList(const flyDocModule_t *pList, const char *szTitle)
{
  flyDocModule_t *pMod = NULL;

  while(pList)
  {
    if(strcmp(szTitle, pList->section.szTitle) == 0)
    {
      pMod = (flyDocModule_t *)pList;
      break;
    }
    pList = pList->pNext;
  }

  return pMod;
}

/*!------------------------------------------------------------------------------------------------
  Counts examples from all modules and classes

  @param    pMainPage   ptr to main page
  @return   none
-------------------------------------------------------------------------------------------------*/
unsigned FlyDocExampleCountAll(flyDoc_t *pDoc)
{
  flyDocModule_t     *pMod;
  flyDocMarkdown_t   *pMarkdown;
  unsigned            count;

  // count module examples
  count = 0;
  pMod = pDoc->pModList;
  while(pMod)
  {
    count += FlyListLen(pMod->section.pExampleList);
    pMod = pMod->pNext;
  }

  // count list examples
  pMod = pDoc->pClassList;
  while(pMod)
  {
    count += FlyListLen(pMod->section.pExampleList);
    pMod = pMod->pNext;
  }

  // count examples in markdown documents
  pMarkdown = pDoc->pMarkdownList;
  while(pMarkdown)
  {
    count += FlyListLen(pMarkdown->section.pExampleList);
    pMarkdown = pMarkdown->pNext;
  }

  // count examples on main page
  if(pDoc->pMainPage)
    count += FlyListLen(pDoc->pMainPage->section.pExampleList);

  return count;
}

/*!------------------------------------------------------------------------------------------------
  Does this line start with @inclass or @ingroup?

  @param    szLine    ptr to a line in the comment header
  @return   TRUE if this line starts with @inclass or @ingroup, FALSE if not
-------------------------------------------------------------------------------------------------*/
bool_t FlyDocIsInGroup(const char *szLine)
{
  flyDocKeyword_t keyword;
  return (FlyDocIsKeyword(szLine, &keyword) && ((keyword == FLYDOC_KEYWORD_INCLASS) ||
          (keyword == FLYDOC_KEYWORD_INCLASS))) ? TRUE : FALSE;
}

/*!------------------------------------------------------------------------------------------------
  Finds the end of this section one of: @class, @defgroup, @fn, @mainpage

  @param    szLine  to line after section open
  @return   ptr to next section or end of string
-------------------------------------------------------------------------------------------------*/
const char * FlyDocSectionEnd(const char *szLine)
{
  flyDocKeyword_t keyword;

  while(*szLine)
  {
    if(FlyDocIsKeyword(szLine, &keyword) && FlyDocIsSection(keyword))
      break;
    szLine = FlyStrLineNext(szLine);
  }

  return szLine;
}

/*!------------------------------------------------------------------------------------------------
  Finds the end of the example given the line AFTER that containing @example

  @param    szLine       ptr to a line containing @example
  @return   ptr to line after end of example
-------------------------------------------------------------------------------------------------*/
const char * FlyDocExampleEnd(const char *szLine)
{
  return FlyMd2HtmlCodeBlkEnd(FlyStrLineSkipBlank(szLine));
}

/*!------------------------------------------------------------------------------------------------
  Parse @color barColor [titleColor]

  1. If barColor or titleColor not present after @color keyword, then they are NULL.
  2. headingColor is text of bar color, e.g. barColor "w3-red" becomes headingColor "w3-text-red"
  2. If barColor is NULL, then so is headingColor

  @param    pDoc        flydoc state
  @param    pszArg      1st argument after @color keyword
  @param    pSection    section to fill in
  @return   none
-------------------------------------------------------------------------------------------------*/
void FlyDocParseColor(flyDoc_t *pDoc, const char *pszArg, flyDocSection_t *pSection)
{
  static const char   szHeadingClass[]  = "w3-text-";

  const char         *psz;
  const char         *szHeadingColor = NULL;
  unsigned            argLen;
  bool_t              fIsMainPage;

  FlyAssert(pDoc && pszArg && pSection);

  fIsMainPage = (pDoc->pMainPage && pSection == &pDoc->pMainPage->section) ? TRUE : FALSE;
  if(pDoc->opts.debug >= FLYDOC_DEBUG_MORE)
    printf("--- ParseColor(%.*s) fIsMainPage %u ---\n", (int)FlyStrLineLen(pszArg), pszArg, fIsMainPage);

  // @color barColor [titleColor]
  // get barcolor and optional title color
  argLen = FlyStrArgLen(pszArg);
  if(argLen)
  {
    pSection->szBarColor = FlyStrAllocN(pszArg, argLen);
    FlyDocAllocCheck(pSection->szBarColor);
    pszArg = FlyStrArgNext(pszArg);
    argLen = FlyStrArgLen(pszArg);
    if(argLen)
    {
      pSection->szTitleColor = FlyStrAllocN(pszArg, argLen);
      FlyDocAllocCheck(pSection->szTitleColor);
      pszArg = FlyStrArgNext(pszArg);
      argLen = FlyStrArgLen(pszArg);
      if(argLen)
      {
        szHeadingColor = pSection->szHeadingColor = FlyStrAllocN(pszArg, argLen);
        FlyDocAllocCheck(pSection->szHeadingColor);
      }
    }
  }

  // e.g. szBarColor "w3-orange" becomes szHeadingColor "w3-text-orange"
  if(pSection->szBarColor && !szHeadingColor)
  {
    psz = pSection->szBarColor + 3;   // skip "w3-"
    pSection->szHeadingColor = FlyDocAlloc(sizeof(szHeadingClass) + strlen(psz));
    {
      strcpy(pSection->szHeadingColor, szHeadingClass);
      strcat(pSection->szHeadingColor, psz);
    }
  }
}

/*!------------------------------------------------------------------------------------------------
  Parse @font body headings

  @param    pszArg              ptr to body arg in line
  @param    ppszFontBody        return value, font for body
  @param    ppszFontHeadings    return value, font for headings
  @return   none
-------------------------------------------------------------------------------------------------*/
void FlyDocParseFont(flyDoc_t *pDoc, const char *pszArg, char **ppszFontBody, char **ppszFontHeadings)
{
  unsigned        argLen;

  if(pDoc->opts.debug >= FLYDOC_DEBUG_MORE)
    printf("--- ParseFont(%.*s) ---\n", (int)FlyStrLineLen(pszArg), pszArg);

  argLen = FlyStrArgLen(pszArg);
  if(argLen)
  {
    *ppszFontBody = FlyStrAllocN(pszArg, argLen);
    pszArg = FlyStrArgNext(pszArg);
    argLen = FlyStrArgLen(pszArg);
    if(argLen)
      *ppszFontHeadings = FlyStrAllocN(pszArg, argLen);  
  }
}

/*!------------------------------------------------------------------------------------------------
  Free the image structure. Does not remove fron any list.

  @param    pImage    ponter to flyDocImage_t structure
  @return   ptr to allocated and filled in image structure
-------------------------------------------------------------------------------------------------*/
flyDocImage_t * FlyDocImageFree(flyDocImage_t *pImage)
{
  if(pImage)
  {
    if(pImage->szLink)
      FlyFree(pImage->szLink);
    FlyFree(pImage);
  }
  return NULL;
}

/*!------------------------------------------------------------------------------------------------
  Allocates and image with a copy of the link.

  Assumes szMdImage actually points to a markdown image, e.g. `[alt text](file.png)`

  @param    pDoc            ptr to document context
  @param    pszMdImage      ptr to markdown image link
  @return   ptr to allocated and filled in image structure
-------------------------------------------------------------------------------------------------*/
flyDocImage_t * FlyDocImageAlloc(flyDoc_t *pDoc, const char *szMdImage)
{
  flyDocImage_t    *pImage;
  flyMdAltLink_t    altLink;

  // this MUST work, as we've already checked that this is an image
  FlyMdAltLink(&altLink, szMdImage);
  FlyAssert(altLink.refType == MD_REF_TYPE_IMAGE && altLink.szLink && altLink.linkLen);

  // should not get here unless it's an image
  pImage = FlyAllocZ(sizeof(*pImage));
  if(pImage)
    pImage->szLink = FlyStrAllocN(altLink.szLink, altLink.linkLen);

  return pImage;
}

/*!------------------------------------------------------------------------------------------------
  Based on a link URL, see if there is a corresponding input image file.

  The image files are collected in a list prior to processing any markdown.

  If the link/URL has a path part it is ignored as it's assumed it's handled outside of flydoc.

  @param    pImgFileList    image file list
  @param    szLink          e.g. "http://foo.com/path/image.png" or "file.jpeg"
  @return   FALSE if expected to find file but didn't
-------------------------------------------------------------------------------------------------*/
bool_t FlyDocImageHasPath(const char *szLink)
{
  return strchr(szLink, '/') ? TRUE : FALSE;
}

/*!------------------------------------------------------------------------------------------------
  Based on a link URL, see if there is a corresponding input image file.

  The image files are collected in a list prior to processing any markdown.

  If the link/URL has a path part it is ignored as it's assumed it's handled outside of flydoc.

  @param    pImgFileList    image file list
  @param    szLink          e.g. "http://foo.com/path/image.png" or "file.jpeg"
  @return   FALSE if expected to find file but didn't
-------------------------------------------------------------------------------------------------*/
bool_t FlyDocImageFileFind(flyDocFile_t  *pImgFileList, const char *szLink)
{
  flyDocFile_t   *pImgFile  = pImgFileList;
  bool_t          fOk       = TRUE;
  bool_t          fFound    = FALSE;

  // ignore links with paths
  if(strchr(szLink, '/') == NULL)
  {
    while(pImgFile)
    {
      // if link is the same as the image file namem then it's found and referenced
      if(strcmp(FlyStrPathNameOnly(pImgFile->szPath), szLink) == 0)
      {
        pImgFile->fReferenced = TRUE;
        fFound = TRUE;
        break;
      }
      pImgFile = pImgFile->pNext;
    }
    if(!fFound)
      fOk = FALSE;
  }

  return fOk;
}

/*!------------------------------------------------------------------------------------------------
  Parse a markdown image link, e.g. `![alt](link "title")`.

  Adds to pDoc->pImageList.

  @param    pDoc            ptr to document context
  @param    pszMdImage      ptr to markdown image reference
  @return   return ptr to after the markdown image reference
-------------------------------------------------------------------------------------------------*/
const char * FlyDocParseImage(flyDoc_t *pDoc, const char *pszMdImage)
{
  flyDocImage_t  *pImage;

  if(pDoc->opts.debug >= FLYDOC_DEBUG_MORE)
    printf("--- ParseImage(%.*s) ---\n", (int)FlyStrLineLen(pszMdImage), pszMdImage);

  // make sure this is a markdown image reference
  FlyAssert(FlyMd2HtmlIsImage(pszMdImage));

  pImage = FlyDocImageAlloc(pDoc, pszMdImage);
  FlyDocAllocCheck(pImage);

  // images might be with path e.g. "https://site.com/path/file.png" or simple, e.g. "file.png"
  if(!FlyDocImageHasPath(pImage->szLink))
  {
    // for simple img links, warn if the image file doesn't exist in the list of input images
    if(!FlyDocImageFileFind(pDoc->pImgFileList, pImage->szLink))
      FlyDocPrintWarningEx(pDoc, szWarningNoImage, pImage->szLink, pDoc->szFile, FlyDocFixupPos(pDoc, pszMdImage));
  }

  pDoc->pImageList = FlyListAppend(pDoc->pImageList, pImage);

  // advance pszArg passed end of image link
  FlyMd2HtmlImage(NULL, UINT_MAX, &pszMdImage);

  return pszMdImage;
}

/*!------------------------------------------------------------------------------------------------
  Parse @logo keyword, which should be followed by an image reference, e.g. `![alt](link "title")`

  @param    pDoc        ptr to document context
  @param    pszArg      ptr to markdown image reference
  @param    ppszLogo    ptr to receive clone of markdown image reference or NULL
  @return   return ptr to after the markdown image reference
-------------------------------------------------------------------------------------------------*/
void FlyDocParseLogo(flyDoc_t *pDoc, const char *pszArg, char **ppszLogo)
{
  const char *pszAfter;

  if(pDoc->opts.debug >= FLYDOC_DEBUG_MORE)
    printf("--- ParseLogo(%.*s) ---\n", (int)FlyStrLineLen(pszArg), pszArg);

  if(!FlyMd2HtmlIsImage(pszArg))
    FlyDocPrintWarningEx(pDoc, szWarningSyntax, NULL, pDoc->szFile, FlyDocFixupPos(pDoc, pszArg));
  else
  {
    pszAfter = FlyDocParseImage(pDoc, pszArg);
    *ppszLogo = FlyStrAllocN(pszArg, pszAfter - pszArg);
  }
}

/*!------------------------------------------------------------------------------------------------
  Parse the `@inclass` or `@ingroup` commands into the pDoc struct.

  Ultimately results in setting the pDoc->pCurMod if worked. May create a stub module or class.

  Prints warning if the syntax is malformed.

  @param    pDoc      Document state
  @param    szLine    ptr to @ingroup line
  @return   TRUE if parsed, FALSE if not @inclass or @ingroup
-------------------------------------------------------------------------------------------------*/
bool_t FlyDocParseInGroup(flyDoc_t *pDoc, const char *szLine)
{
  flyDocKeyword_t   keyword;
  flyDocModule_t   *pList;
  flyDocModule_t   *pMod;
  const char       *szArg;
  char             *szModName = NULL;

  if(pDoc->opts.debug >= FLYDOC_DEBUG_MORE)
    printf("--- ParseInGroup(%.*s) ---\n", (int)FlyStrLineLen(szLine), szLine);

  // it should never get here unless on an @ingrouo or @class line
  szArg = FlyDocIsKeyword(szLine, &keyword);
  FlyAssert(szArg && (keyword == FLYDOC_KEYWORD_INCLASS || keyword == FLYDOC_KEYWORD_INGROUP));

  // make sure the arg is a valid CName, allocate the string if it is
  szModName = FlyDocCNameAlloc(szArg);
  if(!szModName)
  {
    FlyDocPrintWarningEx(pDoc, szWarningSyntax, NULL, pDoc->szFile, FlyDocFixupPos(pDoc, szArg));
    return TRUE;
  }

  // find module
  else
  {
    // does the module already exist? great! we can use it
    pList = (keyword == FLYDOC_KEYWORD_INCLASS) ? pDoc->pClassList : pDoc->pModList;
    pMod = FlyDocModInList(pList, szModName);

    // create stub module/class if it doesn't already exist
    if(pMod == NULL)
    {
      pMod = FlyDocModNew(szModName);
      if(pMod)
      {
        if(keyword == FLYDOC_KEYWORD_INCLASS)
          pDoc->pClassList = FlyDocModListAdd(pDoc->pClassList, pMod, pDoc->opts.fSort);
        else
          pDoc->pModList = FlyDocModListAdd(pDoc->pModList, pMod, pDoc->opts.fSort);
      }
    }

    // adjust current module for all other processing
    if(pMod)
      pDoc->pCurMod = pMod;
  }

  return TRUE;
}

/*!------------------------------------------------------------------------------------------------
  Parse an @example in the text area of a module, class, function, mainpage, markdown file.

  The @example line MUST be followed by a markdown code block:

      @example Unique Title

          code here...

  Examples stay in place in the text area of the given object. They are listed on the mainpage.

  This has the the following duties:

  1. Determine if the example is valid. Return NULL if not.
  2. Determine and return ptr to end of example
  3. Add example to appropriate section.pExampleList

  @param    pDoc        Document state for options
  @param    pSection    module, class, mainpage or markdown doc
  @param    szLine      ptr to @example line
  @param    ppExample   return value: ptr to flyDocExample_t or NULL if failed
  @return   ptr to line after example (including fenced code) or NULL if invalid example
-------------------------------------------------------------------------------------------------*/
const char * FlyDocParseExample(flyDoc_t *pDoc, flyDocSection_t *pSection, const char *szLine, flyDocExample_t **ppExample)
{
  flyDocExample_t  *pExample    = NULL;
  const char       *szExampleEnd = NULL;
  const char       *szTitle;

  if(pDoc->opts.debug)
    printf("--- ParseExample(%.*s) ---\n", (int)FlyStrLineLen(szLine), szLine);

  // there must be a section to put this into, mandatory
  FlyAssert(pSection);

  // get title, mandatory
  szTitle = FlyStrArgNext(FlyStrSkipWhite(szLine));
  if(FlyStrLineIsBlank(szTitle))
  {
    // no title
    FlyDocPrintWarningEx(pDoc, szWarningSyntax, NULL, pDoc->szFile, FlyDocFixupPos(pDoc, szTitle));
    return NULL;
  }

  // get content, which must be in a multi-line code block
  szLine = FlyStrLineSkipBlank(FlyStrLineNext(szLine));
  szExampleEnd = FlyDocExampleEnd(szLine);
  if(szLine == szExampleEnd)
  {
    // no content
    FlyDocPrintWarningEx(pDoc, szWarningEmpty, NULL, pDoc->szFile, FlyDocFixupPos(pDoc, szLine));
  }

  // allocate example
  pExample = FlyDocExampleNew(szTitle);
  FlyDocAllocCheck(pExample);
  *ppExample = pExample;

  return szExampleEnd;
}

/*-------------------------------------------------------------------------------------------------
  Looks in each line of text for 

  Returns ptr to allocated text or NULL if no content.

  It has the following duties:

  1. Parse 0 or more @examples. They get added to the current section (e.g. mainpage, module, class)
  2. Parse @version, @color, @logo which apply to the section. They are removed from text
  3. Removes blank lines from start/end of text area

  @param    pDoc        Document state machine with pExampleList
  @param    szText      a zero terminated string with text
  @param    szEnd       end of text to parse
  @return   ptr to szText or NULL if no content
-------------------------------------------------------------------------------------------------*/
static void MdParseTextForImages(flyDoc_t *pDoc, const char *szText, const char *szTextEnd)
{
  const char     *psz;
  const char     *szLine;
  unsigned        i;

  szLine = szText;
  while(*szLine && szLine < szTextEnd)
  {
    // skip code block lines. but don't skip lists or tables or block quotes
    if(FlyMd2HtmlIsCodeBlk(szLine, NULL))
    {
      FlyMd2HtmlCodeBlk(NULL, SIZE_MAX, &szLine, NULL, NULL);
      continue;
    }

    // already processed @logo, don't process twice
    if(FlyDocIsKeyword(szLine, NULL))
    {
      szLine = FlyStrLineNext(szLine);
      continue;
    }

    // look in a single line for images, e.g. ![alt text](link.png). There may be more than 1.
    psz = szLine;
    i = 8;
    do
    {
      psz = FlyMdNPBrk(psz, FlyStrLineEnd(psz), "!");
      if(psz)
      {
        // add to list
        if(FlyMd2HtmlIsImage(psz))
          psz = FlyDocParseImage(pDoc, psz);
        else
          ++psz;
        ++i;
      }
    } while(i < 16 && psz);

    szLine = FlyStrLineNext(szLine);
  }
}

/*-------------------------------------------------------------------------------------------------
  Parse style keywords for the section: @color, @font, @logo, @version

  Will print error messages if syntax is wrong for 

  @param    pDoc        Document state machine with pExampleList
  @param    pSection    the section to affect
  @param    szArg       ptr to argument after @keyword
  @param    keyword     the keyword found
  @return   none
-------------------------------------------------------------------------------------------------*/
static void MdParseStyleKeyword(flyDoc_t *pDoc, flyDocSection_t *pSection, const char *pszArg, flyDocKeyword_t keyword)
{
  if(keyword == FLYDOC_KEYWORD_COLOR)
    FlyDocParseColor(pDoc, pszArg, pSection);
  else if(keyword == FLYDOC_KEYWORD_FONT)
    FlyDocParseFont(pDoc, pszArg, &pSection->szFontBody, &pSection->szFontHeadings);
  else if(keyword == FLYDOC_KEYWORD_LOGO)
    FlyDocParseLogo(pDoc, pszArg, &pSection->szLogo);
  else if(keyword == FLYDOC_KEYWORD_VERSION)
    pSection->szVersion = FlyDocAllocToLineEnd(pszArg);
}

/*!------------------------------------------------------------------------------------------------
  Parse the text area of a class, module, or function or mainpage.

  Returns ptr to allocated text or NULL if no content.

  It has the following duties:

  1. Parse 0 or more @examples. They get added to the current section (e.g. mainpage, module, class)
  2. Parse style keywords: @version, @color, @logo which apply to the section
  3. Parse text for any images so local images are marked for copy to output folder/

  @param    pDoc        Document state machine with pExampleList
  @param    szLine      ptr to @example line
  @param    szEnd       end of text to parse
  @return   ptr to szText or NULL if no content
-------------------------------------------------------------------------------------------------*/
char * FlyDocParseText(flyDoc_t *pDoc, flyDocSection_t *pSection, const char *szStart, const char *szEnd)
{
  char             *szText        = NULL;
  const char       *szLine        = NULL;
  const char       *szEndExample  = NULL;
  const char       *pszArg        = NULL;
  char             *psz           = NULL;
  flyDocExample_t  *pExample      = NULL;
  flyDocKeyword_t   keyword;
  unsigned          size;
  unsigned          len;

  // check parmeters
  FlyAssert(szStart && szEnd && szStart <= szEnd);

  if(pDoc->opts.debug >= FLYDOC_DEBUG_MORE)
    printf("--- FlyDocParseText(%.*s) ---\n", (int)FlyStrLineLen(szStart), szStart);
  if(pDoc->opts.debug >= FLYDOC_DEBUG_MAX)
    FlyStrDump(szStart, (szEnd - szStart));

  // parse @example, @logo, @version, @color, @font
  // all these lines will be removed or modified in the text
  // also parse any images found on the line
  szLine = szStart;
  while(*szLine && szLine < szEnd)
  {
    // parse any @keywords
    pszArg = FlyDocIsKeyword(szLine, &keyword);
    if(pszArg)
    {
      if(keyword == FLYDOC_KEYWORD_EXAMPLE)
      {
        // parse the example and add to pSection->pExampleLine
        szEndExample = FlyDocParseExample(pDoc, pSection, szLine, &pExample);
        if(szEndExample)
        {
          szLine = szEndExample;
          pSection->pExampleList = FlyListAppend(pSection->pExampleList, pExample);
          continue;
        }
      }
      else
        MdParseStyleKeyword(pDoc, pSection, pszArg, keyword);
    }
    szLine = FlyStrLineNext(szLine);
  }

  // allocate space for text
  szLine = szStart;
  size = FlyDocTextLenCalc(szLine, szEnd);
  szText = FlyAllocZ(size);
  FlyDocAllocCheck(szText);

  // add appropriate lines into text (will remove keyword lines)
  psz = szText;
  szLine = szStart;
  while(*szLine && szLine < szEnd)
  {
    // throw out keywords lines, as they've already been processed
    // exception is example and unknown, both which are left as-is
    // @example will be adjusted in flydochtml.c or flydocmd.c
    if(FlyDocIsKeyword(szLine, &keyword) && !((keyword == FLYDOC_KEYWORD_EXAMPLE || keyword == FLYDOC_KEYWORD_UNKNOWN)))
    {
      szLine = FlyStrLineNext(szLine);
      continue;
    }

    // copy the line from the original text to our modified one
    len = FlyStrLineLenEx(szLine);
    memcpy(psz, szLine, len);
    psz += len;
    szLine = FlyStrLineNext(szLine);
  }

  FlyStrLineBlankRemove(szText);
  if(strlen(szText) == 0)
  {
    FlyFree(szText);
    szText = NULL;
  }

  // reecord found images in case image files need to be copied and for stats
  if(szText)
    MdParseTextForImages(pDoc, szText, szText + strlen(szText));

  return szText;
}

/*!------------------------------------------------------------------------------------------------
  Parse a @class or @defgroup section of the header into the pDoc structure

  @param    pDoc            document state
  @param    szSection       e.g. ptr to "@defgroup  ModuleName   Brief Description of Module"
  @param    szSectionEnd    ptr start of new section or end of doc comment
  @return   none
-------------------------------------------------------------------------------------------------*/
void FlyDocParseModule(flyDoc_t *pDoc, const char *szSection, const char *szSectionEnd, bool_t fClass)
{
  flyDocModule_t   *pList;
  flyDocModule_t   *pMod;
  const char       *szTitle     = NULL;
  const char       *szSubtitle  = NULL;
  const char       *szLine;
  char             *szCName     = NULL;
  bool_t            fWorked     = TRUE;

  if(pDoc->opts.debug)
    printf("--- Parse%s(%.*s) ---\n", fClass ? "Class" : "Module", (int)FlyStrLineLen(szSection), szSection);

  // pattern is @defgroup  ModuleName  Brief Description of Module
  //         or @class     ClassName   Brief Description of Class
  if(!FlyDocGetNameDescription(szSection, &szTitle, &szSubtitle))
  {
    FlyDocPrintWarningEx(pDoc, szWarningSyntax, NULL, pDoc->szFile, FlyDocFixupPos(pDoc, szSection));
    fWorked = FALSE;
  }

  // Duplicate modules ignored. Not considered duplicate if title only as created by @ingroup or @inclass
  if(fWorked)
  {
    szCName = FlyDocCNameAlloc(szTitle);
    if(!szCName)
    {
      FlyDocPrintWarningEx(pDoc, szWarningSyntax, NULL, pDoc->szFile, FlyDocFixupPos(pDoc, szSection));
      fWorked = FALSE;
    }
    else
    {
      pList = fClass ? pDoc->pClassList : pDoc->pModList;
      pMod = FlyDocModInList(pList, szCName);
      if(pMod && (pMod->section.szSubtitle || pMod->section.szText))
      {
        FlyDocPrintWarningEx(pDoc, szWarningDuplicate, szCName, pDoc->szFile, FlyDocFixupPos(pDoc, szSection));
        fWorked = FALSE;
      }
    }
  }

  // create the module if needed
  if(fWorked && pMod == NULL)
  {
    pMod = FlyDocModNew(szCName);
    FlyDocAllocCheck(pMod);
    FlyDocDupCheck(pDoc, pMod->section.szTitle, FlyDocFixupPos(pDoc, szSection));
    if(fClass)
      pDoc->pClassList = FlyDocModListAdd(pDoc->pClassList, pMod, pDoc->opts.fSort);
    else
      pDoc->pModList = FlyDocModListAdd(pDoc->pModList, pMod, pDoc->opts.fSort);
  }

  if(fWorked)
  {
    FlyFreeIf(pMod->section.szSubtitle);
    pMod->section.szSubtitle = FlyDocAllocToLineEnd(szSubtitle);
    FlyDocAllocCheck(pMod->section.szSubtitle);
  }
 
  // this is now the current module
  pDoc->pCurMod = pMod;

  // add in text section. This will also parse examples.
  szLine = FlyStrLineNext(szSection);
  pMod->section.szText = FlyDocParseText(pDoc, &pMod->section, szLine, szSectionEnd);

  FlyFreeIf(szCName);

  // print newly created module if we're debugging
  if(pDoc->opts.debug)
    FlyDocPrintModule(pMod, pDoc->opts.debug);
}

/*!------------------------------------------------------------------------------------------------
  Check in all lists for a duplicate output filename.

  For example, if there is a module, class and document, all named "Foo", it would issue 2 warnings
  indicating the 2nd two are duplicates.

  This is case insensitive because both macOS and Windows are case insensitive for filenames.

  @param    pDoc      document state
  @param    szTitle   title of a module, class or document
  @param    szPos     position in file where duplicate occurred
  @return   none
-------------------------------------------------------------------------------------------------*/
void FlyDocDupCheck(flyDoc_t *pDoc, const char *szTitle, const char *szPos)
{
  flyDocModule_t    *pMod;
  flyDocMarkdown_t  *pDocument;
  flyStrSmart_t     title;
  flyStrSmart_t     thisTitle;
  char             *psz;
  size_t            nPages    = 0;
  bool_t            fIsDup    = FALSE;

  // title might be in myfile.md form, convert to just myfile for comparison
  FlyStrSmartInitEx(&title, 128);
  FlyStrSmartInitEx(&thisTitle, 128);
  FlyStrSmartCpy(&title, szTitle);
  psz = (char *)FlyStrPathExt(title.sz);
  if(psz)
    *psz = '\0';

  // check module list
  pMod = pDoc->pModList;
  while(!fIsDup && pMod)
  {
    if(strcasecmp(title.sz, pMod->section.szTitle) == 0)
      fIsDup = TRUE;
    ++nPages;
    pMod = pMod->pNext;
  }

  // check class list
  pMod = pDoc->pClassList;
  while(!fIsDup && pMod)
  {
    if(strcasecmp(title.sz, pMod->section.szTitle) == 0)
      fIsDup = TRUE;
    ++nPages;
    pMod = pMod->pNext;
  }

  // check markdown document list
  pDocument = pDoc->pMarkdownList;
  while(pDocument)
  {
    // title is in myfile.md form, convert to myfile for comparison
    FlyStrSmartCpy(&thisTitle, pDocument->section.szTitle);
    psz = FlyStrPathExt(thisTitle.sz);
    if(psz)
      *psz = '\0';

    if(strcasecmp(title.sz, thisTitle.sz) == 0)
      fIsDup = TRUE;
    ++nPages;
    pDocument = pDocument->pNext;
  }

  // check for conflict with index (mainpage), a mainpage is always created if more than 1 page
  if((strcasecmp(title.sz, "index") == 0) && (pDoc->pMainPage || nPages > 1))
    fIsDup = TRUE;

  if(fIsDup)
  {
    if(szPos)
      FlyDocPrintWarningEx(pDoc, szWarningDuplicate, szTitle, pDoc->szFile, szPos);
    else
      FlyDocPrintWarning(pDoc, szWarningDuplicate, title.sz);
  }
}

/*!------------------------------------------------------------------------------------------------
  Parse the mainpage section.

  Ignores with warning mainpage already exists. There shall be only one!

  Example

      @mainpage My Project
      @version  0.9.53

      A single line subtitle that describes the project in brief

      Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut
      labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco
      laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure dolor in reprehenderit in
      voluptate velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat
      cupidatat non proident, sunt in culpa qui officia deserunt mollit anim id est laborum.

  @param    pDoc            ptr to document context
  @param    szHdr           ptr to stripped doc comment header
  @param    szSectionEnd    ptr start of new section or end of doc comment
  @return   none
-------------------------------------------------------------------------------------------------*/
void FlyDocParseMainPage(flyDoc_t *pDoc, const char *szSection, const char *szSectionEnd)
{
  flyDocMainPage_t *pMainPage;
  flyDocSection_t  *pSection;
  const char       *szLine;
  const char       *pszArg;
  flyDocKeyword_t   keyword;

  if(pDoc->opts.debug)
    printf("--- ParseMainPage(%.*s) ---\n", (int)FlyStrLineLen(szSection), szSection);

  if(pDoc->pMainPage)
  {
    FlyDocPrintWarningEx(pDoc, szWarningDuplicate, "mainpage", pDoc->szFile, FlyDocFixupPos(pDoc, szSection));
    return;
  }

  pMainPage = FlyAlloc(sizeof(*pMainPage));
  if(pMainPage)
  {
    pDoc->pMainPage = pMainPage;
    memset(pMainPage, 0, sizeof(*pMainPage));

    // the title follows the mainpage keyword to end of line
    pMainPage->section.szTitle = FlyDocAllocToLineEnd(FlyStrArgNext(FlyStrSkipWhite(szSection)));
    szSection = FlyStrLineNext(szSection);

    // check for subtitle and section settings
    szLine = szSection;
    pSection = &pMainPage->section;
    while(*szLine && szLine < szSectionEnd)
    {
      pszArg = FlyDocIsKeyword(szLine, &keyword);
      if(pszArg)
        MdParseStyleKeyword(pDoc, pSection, pszArg, keyword);
      else if(!FlyStrLineIsBlank(szLine))
      {
        // subtitle must be a single line, otherwise no subtitle
        if(FlyStrLineIsBlank(FlyStrLineNext(szLine)))
        {
          pMainPage->section.szSubtitle = FlyDocAllocToLineEnd(szLine);
          szSection = FlyStrLineNext(szLine);
        }
        break;
      }
      szLine = FlyStrLineNext(szLine);
    }

    if(szSection < szSectionEnd)
      pMainPage->section.szText = FlyDocParseText(pDoc, &pMainPage->section, szSection, szSectionEnd);

    if(pDoc->opts.debug)
      FlyDocPrintMainPage(pMainPage, pDoc->opts.debug);
  }
}

/*!------------------------------------------------------------------------------------------------
  Parse the flydoc comment header for a function/method and add to current class or module.

  * Function prototype for Python doc strings come BEFORE the header
  * All other functions prototypes come AFTER the header
  * prototype may be passed in (from @fn)
  * If no function after the header, and @inclass or @ingroup, then no warning

  @param    pDoc            ptr to document context
  @param    szHdr           ptr to stripped, '\0' terminated header
  @param    szEnd           end of header to seearch
  @param    szRawHdr        ptr to raw header in file
  @param    szPrototype     Don't look for prototype, already provided
  @return   none
-------------------------------------------------------------------------------------------------*/
void FlyDocParseFunction(flyDoc_t *pDoc, const char *szSection, const char *szSectionEnd, flyStrHdr_t *pHdr, const char *szPrototype)
{
  const char       *szLine      = NULL;
  char             *psz         = NULL;
  const char       *szFuncName  = NULL;
  const char       *szBrief     = NULL;
  flyDocKeyword_t   keyword;
  flyDocFunc_t     *pFunc       = NULL;
  unsigned          protoLen;
  unsigned          sizeProto = 0;

  if(pDoc->opts.debug >= FLYDOC_DEBUG_MORE)
  {
    printf("--- ParseFunction(%.*s) ---\n", (int)FlyStrLineLen(FlyStrHdrContentStart(pHdr)), FlyStrHdrContentStart(pHdr));
    if(pDoc->opts.debug >= FLYDOC_DEBUG_MAX)
      FlyStrDump(FlyStrHdrContentStart(pHdr), (unsigned)(FlyStrHdrContentEnd(pHdr) - FlyStrHdrContentStart(pHdr)) + 1);
  }

  // first things first, handle @inclass, @ingroup and brief
  szLine = szSection;
  while(*szLine && szLine < szSectionEnd)
  {
    if(FlyDocIsKeyword(szLine, &keyword))
    {
      // process @inclass or @ingroup (may change current module)
      if((keyword == FLYDOC_KEYWORD_INCLASS) || (keyword == FLYDOC_KEYWORD_INGROUP))
        FlyDocParseInGroup(pDoc, szLine);
    }

    // 1st non-blank line is the brief for the function
    else if(szBrief == NULL && !FlyStrLineIsBlank(szLine))
      szBrief = szLine;
    szLine = FlyStrLineNext(szLine);
  }

  // no class or module defined either earlier in file or in this comment
  // drop the function as we've no place to put it
  if(!pDoc->pCurMod)
  {
    FlyDocPrintWarningEx(pDoc, szWarningNoModule, NULL, pDoc->szFile, FlyDocFixupPos(pDoc, szSection));
    return;
  }

  // if prototype is not give, find prototype
  if(szPrototype == NULL)
  {
    // Python doc string prototypes are BEFORE comment
    if(FlyStrHdrType(pHdr) == FLYSTRHDR_TYPE_PYDOC)
      szLine = FlyStrLinePrev(pDoc->szFile, FlyStrRawHdrLine(pHdr));

    // normal prototypes follow comment
    else
    {
      // find prototype and function CName (first non-blank line after header)
      szLine = FlyStrRawHdrEnd(pHdr);
      while(*szLine && FlyStrLineIsBlank(szLine))
        szLine = FlyStrLineNext(szLine);
    }

    szPrototype = FlyStrSkipWhite(szLine);
  }

  // determine length of prototype
  protoLen = FlyStrFnProtoLen(szPrototype, &szFuncName);

  // no prototype, don't process as a function
  if(!protoLen || szFuncName == NULL)
  {
    FlyDocPrintWarningEx(pDoc,
      (FlyStrHdrType(pHdr) == FLYSTRHDR_TYPE_PYDOC) ? szWarningBadDocStr : szWarningNoFunction,
      NULL, pDoc->szFile, FlyDocFixupPos(pDoc, szSection));
    return;
  }

  // add function into the module list
  pFunc = FlyDocFuncNew(szFuncName);
  FlyDocAllocCheck(pFunc);
  pDoc->pCurMod->pFuncList = FlyDocFuncListAdd(pDoc->pCurMod->pFuncList, pFunc, pDoc->opts.fSort);

  // allocate a copy of the szBrief line
  if(szBrief)
    pFunc->szBrief = FlyStrAllocN(szBrief, FlyStrLineLen(szBrief));

  // determine size of prototype paragraph, which includes the @params and @return
  sizeProto = protoLen + strlen(szTwoLines) + 1;
  szLine = szSection;
  while(*szLine && szLine < szSectionEnd)
  {
    if(FlyDocIsKeyword(szLine, &keyword) && FlyDocIsKeywordProto(keyword))
      sizeProto += FlyStrLineLenEx(szLine) + strlen(szFlyDocExtra);
    szLine = FlyStrLineNext(szLine);
  }

  // allocate and copy prototype paragraph, which includes @param and @return
  pFunc->szPrototype = FlyAlloc(sizeProto);
  if(pFunc->szPrototype)
  {
    memset(pFunc->szPrototype, 0, sizeProto);
    psz = pFunc->szPrototype;

    // copy in function prototype
    memcpy(psz, szPrototype, protoLen);
    psz += protoLen;
    strcpy(psz, szTwoLines);
    psz += strlen(szTwoLines);

    // copy in any @keyword lines like @param and @return into prototype lines
    if(szBrief)
      szLine = FlyStrLineNext(szBrief);
    else
      szLine = szSection;
    while(*szLine)
    {
      if(FlyDocIsKeyword(szLine, &keyword) && FlyDocIsKeywordProto(keyword))
        psz = FlyDocExtraLineCopy(psz, szLine);
      szLine = FlyStrLineNext(szLine);
    }
    *psz = '\0';
    FlyAssert((psz - pFunc->szPrototype) < sizeProto);
    FlyStrLineBlankRemove(pFunc->szPrototype);

    // determine language based on filename
    pFunc->szLang = FlyStrPathLang(pDoc->szPath);
  }

  // allocate and copy the text (notes) section of the function
  // may also add examples to module/class
  if(szBrief)
    szLine = FlyStrLineNext(szBrief);
  else
    szLine = szSection;
  FlyAssert(pDoc->pCurMod);
  pFunc->szText = FlyDocParseText(pDoc, &pDoc->pCurMod->section, szLine, szLine + strlen(szLine));

  if((pDoc->opts.debug >= FLYDOC_DEBUG_MORE) && pFunc)
    FlyDocPrintFunc(pFunc, FLYDOC_DEBUG_SOME, 2);
}

/*!------------------------------------------------------------------------------------------------
  Parse the flydoc block comment header, stripped to just markdown and keywords.

  Note: the comment header may contain multiple sections.

  @param    pDoc      ptr to document context
  @param    szHdr     ptr to stripped header
  @param    pHdr      ptr to all info about the header (block comment), or NULL if a markdown file
  @return   none
-------------------------------------------------------------------------------------------------*/
void FlyDocParseHdr(flyDoc_t *pDoc, const char *szHdr, flyStrHdr_t *pHdr)
{
  const char       *szLine;
  const char       *szSectionEnd;
  const char       *pszArg;
  char             *szPrototype;
  flyDocKeyword_t   keyword;
  bool_t            fFoundText = FALSE;

  if(pDoc->opts.debug)
  {
    unsigned  row;
    unsigned  col;
    szLine = FlyDocFixupPos(pDoc, szHdr);
    row = FlyStrLinePos(pDoc->szFile, szLine, &col);
    printf("--- FlyDocParseHdr(%u:%u:%.*s) ---\n", row, col, (int)FlyStrLineLen(szHdr), szHdr);
  }

  // one more doc comment header
  ++pDoc->nDocComments;

  // process the header
  szLine = szHdr;
  while(szLine && *szLine)
  {
    // @mainpage, @class, @defgroup, @fn, @ingroup, @inclass
    pszArg = FlyDocIsKeyword(szLine, &keyword);
    if(pszArg && (keyword == FLYDOC_KEYWORD_INGROUP || keyword == FLYDOC_KEYWORD_INCLASS))
    {
      FlyDocParseInGroup(pDoc, szLine);
      szLine = FlyStrLineNext(szLine);
      continue;
    }

    else if(pszArg && FlyDocIsSection(keyword))
    {
      fFoundText = FALSE;
      szSectionEnd = FlyDocSectionEnd(FlyStrLineNext(szLine));

      if(pDoc->opts.debug >= FLYDOC_DEBUG_MAX)
      {
        printf("--- section %p => %p ---\n", szLine, szSectionEnd);
        FlyStrDump(szLine, szSectionEnd - szLine);
      }

      switch(keyword)
      {
        case FLYDOC_KEYWORD_CLASS:
          FlyDocParseModule(pDoc, szLine, szSectionEnd, PARSE_CLASS);
        break;
        case FLYDOC_KEYWORD_DEFGROUP:
          FlyDocParseModule(pDoc, szLine, szSectionEnd, PARSE_MODULE);
        break;
        case FLYDOC_KEYWORD_FN:
          if(pHdr)
          {
            szPrototype = FlyStrAllocN(pszArg, FlyStrLineLen(pszArg));
            FlyDocAllocCheck(szPrototype);
            FlyDocParseFunction(pDoc, szLine, szSectionEnd, pHdr, szPrototype);
            FlyFree(szPrototype);
          }
        break;
        case FLYDOC_KEYWORD_MAINPAGE:
          FlyDocParseMainPage(pDoc, szLine, szSectionEnd);
        break;
        default:
          FlyAssertFail("FlyDocParseHdr: FlyDocIsSection() and case statements do NOT agree\n");
        break;
      }
      szLine = szSectionEnd;
    }

    // empty line or not in a section, ignore it for now
    else
    {
      if(!FlyStrLineIsBlank(szLine))
        fFoundText = TRUE;
      szLine = FlyStrLineNext(szLine);
    }
  }

  // A function doc comment may have no keywords, example:
  //
  // /*!
  //   calculate area of a circle
  // */
  // math_circle_area(double r)
  // {
  //   return PI * r * r;
  // }
  if(fFoundText && pHdr)
    FlyDocParseFunction(pDoc, szHdr, szHdr + strlen(szHdr), pHdr, NULL);
}

/*!------------------------------------------------------------------------------------------------
  Create a markdown structure. Fills in szFile and szTitle only.

  Warning: szFile MUST be persistent!

  @param    szFile    ptr to markdown file in memory (MUST be persistent)
  @param    szTitle   header text, will cloned
  @return   ptr to markdown structure or NULL if no memory
-------------------------------------------------------------------------------------------------*/
flyDocMarkdown_t * FlyDocMarkdownNew(const char *szFile, const char *szTitle)
{
  flyDocMarkdown_t *pMarkdown;

  pMarkdown = FlyAlloc(sizeof(*pMarkdown));
  if(pMarkdown)
  {
    memset(pMarkdown, 0, sizeof(*pMarkdown));
    pMarkdown->szFile = szFile;
    if(szTitle)
      pMarkdown->section.szTitle = FlyStrClone(szTitle);
  }
  return pMarkdown;
}

/*!------------------------------------------------------------------------------------------------
  Compare two flyDocMarkdown_t structures

  @param    pThis    ptr to document context
  @param    pThat  ptr to file path (relative or absolute)
  @return   none
-------------------------------------------------------------------------------------------------*/
int FlyDocMarkdownCmp(const void *pThis, const void *pThat)
{
  const flyDocMarkdown_t *pMd1 = pThis;
  const flyDocMarkdown_t *pMd2 = pThat;

  FlyAssert(pMd1 && pMd2);
  FlyAssert(pMd1->section.szTitle && pMd2->section.szTitle);

  return strcmp(pMd1->section.szTitle, pMd2->section.szTitle);
}

/*!------------------------------------------------------------------------------------------------
  Create a new markdown header structure

  @param    pDoc    ptr to flyDocMdHeader_t
  @param    szPath  ptr to flyDocMdHeader_t
  @return   < 0 less than, 0 equal, > 0 greater than
-------------------------------------------------------------------------------------------------*/
flyDocMdHdr_t * FlyDocMdHdrNew(const char *szTitle)
{
  flyDocMdHdr_t *pMdHdr;

  pMdHdr = FlyAlloc(sizeof(*pMdHdr));
  if(pMdHdr)
  {
    memset(pMdHdr, 0, sizeof(*pMdHdr));
    pMdHdr->szTitle = szTitle;
  }
  return pMdHdr;
}

/*!-------------------------------------------------------------------------------------------------
  Parse a markdown file into the pDoc->pMarkdownList

  @param    pDoc    ptr to document context
  @param    szFile  ptr to file contents
  @return   TRUE if worked, FALSE if not a markdown file
-------------------------------------------------------------------------------------------------*/
bool_t FlyDocParseMarkdownFile(flyDoc_t *pDoc, const char *szFile)
{
  flyDocMarkdown_t *pMarkdown;
  flyDocSection_t  *pSection;
  flyDocMdHdr_t    *pMdHdr;
  flyDocExample_t  *pExample;
  char             *szHeading;
  const char       *szLine;
  bool_t            fGotHdr = FALSE;
  bool_t            fWorked = TRUE;
  flyDocKeyword_t   keyword;
  const char       *pszArg;


  if(pDoc->opts.debug)
    printf("--- FlyDocParseMarkdown(%s) szFile %p len %zu ---\n", pDoc->szPath, szFile, strlen(szFile));

  // make sure we can give proper error messages
  FlyAssert(szFile && pDoc->szFile == szFile);

  // if the 1st line is a section keyword, process as a doc comment
  pszArg = FlyDocIsKeyword(szFile, &keyword);
  if(pszArg && FlyDocIsSection(keyword))
  {
    FlyDocParseHdr(pDoc, szFile, NULL);
    return TRUE;
  }

  // allocate a new markdown file with title of the filename only
  pMarkdown = FlyDocMarkdownNew(szFile, FlyStrPathNameOnly(pDoc->szPath));
  FlyDocAllocCheck(pMarkdown);
  FlyDocDupCheck(pDoc, pMarkdown->section.szTitle, NULL);

  // add into list of documents
  if(pDoc->opts.fSort)
    pDoc->pMarkdownList = FlyListAddSorted(pDoc->pMarkdownList, pMarkdown, FlyDocMarkdownCmp);
  else
    pDoc->pMarkdownList = FlyListAppend(pDoc->pMarkdownList, pMarkdown);

  pSection  = &pMarkdown->section;
  pSection->szText = (char *)szFile;

  szLine = szFile;
  while(szLine && *szLine)
  {
    // process @logo, @version, @color, @font
    pszArg = FlyDocIsKeyword(szLine, &keyword);
    if(pszArg)
    {
      if(keyword == FLYDOC_KEYWORD_EXAMPLE)
      {
        szLine = FlyDocParseExample(pDoc, &pMarkdown->section, szLine, &pExample);
        if(pExample)
          pMarkdown->section.pExampleList = FlyListAppend(pMarkdown->section.pExampleList, pExample);
      }
      else
      {
        MdParseStyleKeyword(pDoc, pSection, pszArg, keyword);
      }
    }

    else if(FlyMd2HtmlIsCodeBlk(szLine, NULL))
    {
      szLine = FlyMd2HtmlCodeBlkEnd(szLine);
      continue;
    }

    else if(FlyMd2HtmlIsHeading(szLine, NULL))
    {
      // allocate title
      pszArg = FlyMd2HtmlHeadingText(szLine);
      szHeading = FlyStrAllocN(pszArg, FlyStrLineLen(pszArg));
      FlyDocAllocCheck(szHeading);

      // subtitle is 1st header
      if(!fGotHdr)
      {
        FlyFreeIf(pMarkdown->section.szSubtitle);
        pMarkdown->section.szSubtitle = szHeading;
        fGotHdr = TRUE;
      }

      pMdHdr = FlyDocMdHdrNew(szHeading);
      pMarkdown->pHdrList = FlyListAppend(pMarkdown->pHdrList, pMdHdr);
    }

    szLine = FlyStrLineNext(szLine);
  }

  if(fWorked)
    MdParseTextForImages(pDoc, szFile, szFile + strlen(szFile));

  return fWorked;
}

/*!------------------------------------------------------------------------------------------------
  Parse a source file into flydoc.

  May create multiple modules, classes, functions, methods, examples, or file may have no flydoc
  content whatsoever.

  @param    pDoc    ptr to document context
  @param    szPath  ptr to file path (relative or absolute)
  @return   TRUE if worked, FALSE if can't read file
-------------------------------------------------------------------------------------------------*/
bool_t FlyDocParseSrcFile(flyDoc_t *pDoc, const char *szFile)
{
  const char   *szRawHdr;
  const char   *szLine;
  char         *szHdr;
  flyStrHdr_t   hdr;
  size_t        len;
  bool_t        fWorked = TRUE;

  if(pDoc->opts.debug)
    printf("--- FlyDocParseSrcFile(%s)---\n", pDoc->szPath);

  FlyAssert(szFile && pDoc->szFile == szFile);

  // don't know the current module/class for functions at this point
  pDoc->pCurMod = NULL;
  szLine = szFile;
  while(TRUE)
  {
    // if no more flydoc headers, we're done
    szRawHdr = FlyStrHdrFind(szLine, TRUE, &hdr);
    if(!szRawHdr)
      break;
    pDoc->pCurHdr = &hdr;

    // allocate a clean header 
    len = FlyStrHdrCpy(NULL, &hdr, SIZE_MAX);
    if(len)
    {
      szHdr = FlyAlloc(len + 1);
      FlyDocAllocCheck(szHdr);
      FlyStrHdrCpy(szHdr, &hdr, len + 1);
      pDoc->szCurHdr = szHdr;
      FlyDocParseHdr(pDoc, szHdr, &hdr);
      FlyFree(szHdr);
    }

    // on to next header
    szLine = FlyStrRawHdrEnd(&hdr);
    pDoc->pCurHdr  = NULL;
    pDoc->szCurHdr = NULL;
  }

  return fWorked;
}

/*!------------------------------------------------------------------------------------------------
  Parse a file into flydoc. May create functions, module, class objects, or file may have no flydoc
  content whatsoever.

  @param    pDoc    ptr to document context
  @param    szPath  ptr to file path (relative or absolute)
  @return   TRUE if worked, FALSE if can't read file
-------------------------------------------------------------------------------------------------*/
bool_t FlyDocParseFile(flyDoc_t *pDoc, const char *szPath)
{
  typedef enum
  {
    FLYDOC_FILE_TYPE_NONE = 0,
    FLYDOC_FILE_TYPE_IMG,
    FLYDOC_FILE_TYPE_MARKDOWN,
    FLYDOC_FILE_TYPE_SRC,
  } fileType_t;

  fileType_t    fileType;
  bool_t        fWorked   = TRUE;

  if(pDoc->opts.debug)
    printf("--- FlyDocParseFile(%s)---\n", szPath);

  // how we process depends on file extension, not content
  fileType  = FLYDOC_FILE_TYPE_NONE;
  if(FlyStrPathHasExt(szPath, pDoc->opts.szExts))
    fileType = FLYDOC_FILE_TYPE_SRC;
  else if(FlyStrPathHasExt(szPath, m_szMarkdownExts))
    fileType = FLYDOC_FILE_TYPE_MARKDOWN;

  // ignore files we don't know the extension of
  pDoc->pCurMod = NULL;
  pDoc->pCurHdr = NULL;
  *pDoc->szPath = '\0';
  pDoc->szFile  = NULL;
  if(fileType != FLYDOC_FILE_TYPE_NONE)
  {
    ++pDoc->nFiles;
    FlyStrZCpy(pDoc->szPath, szPath, sizeof(pDoc->szPath));
    if(pDoc->opts.verbose >= FLYDOC_VERBOSE_MORE)
      printf("%s\n", pDoc->szPath);

    // read in the file
    pDoc->szFile = FlyFileRead(szPath);
    if(!pDoc->szFile || strlen(pDoc->szFile) == 0)
      FlyDocPrintWarning(pDoc, szWarningReadFile, szPath);
    else
    {
      if(fileType == FLYDOC_FILE_TYPE_SRC)
        fWorked = FlyDocParseSrcFile(pDoc, pDoc->szFile);
      else if(fileType == FLYDOC_FILE_TYPE_MARKDOWN)
        fWorked = FlyDocParseMarkdownFile(pDoc, pDoc->szFile);
    }

    if(fileType != FLYDOC_FILE_TYPE_MARKDOWN && pDoc->szFile)
      FlyFree((char *)pDoc->szFile);

    pDoc->szFile = NULL;
  }

  return fWorked;
}

/*!------------------------------------------------------------------------------------------------
  Updates statistics fields in flydoc from lists

  Note: doesn't calculate nFiles, nDocComments, nWarnings as these are calculated on the fly.

  @param    pDoc      A document state machine initialized with FlyDocInit()
  @return   none
-------------------------------------------------------------------------------------------------*/
void FlyDocStatsUpdate(flyDoc_t *pDoc)
{
  flyDocModule_t *pMod;

  pDoc->nModules   = FlyListLen(pDoc->pModList);
  pDoc->nFunctions = 0;
  pMod = pDoc->pModList;
  while(pMod)
  {
    pDoc->nFunctions += (unsigned)FlyListLen(pMod->pFuncList);
    pMod = pMod->pNext;
  }
  pDoc->nClasses   = FlyListLen(pDoc->pClassList);
  pDoc->nMethods   = 0;
  pMod = pDoc->pClassList;
  while(pMod)
  {
    pDoc->nMethods += (unsigned)FlyListLen(pMod->pFuncList);
    pMod = pMod->pNext;
  }
  pDoc->nExamples  = FlyDocExampleCountAll(pDoc);
  pDoc->nDocuments = FlyListLen(pDoc->pMarkdownList);
  pDoc->nImages    = FlyListLen(pDoc->pImageList);
}

/*!------------------------------------------------------------------------------------------------
  Allocates an image file structure and clones the path. Also adds to image file list in pDoc.

  @param    pDoc      document context with pDoc->pImgFileList
  @param    szPath    e.g. `file.jpg` , `../file2.png` or `~/folder/file3.gif`
  @return   pointer to image file or NULL if failed to allocate memory
-------------------------------------------------------------------------------------------------*/
void FlyDocImgFileListAdd(flyDoc_t *pDoc, const char *szPath)
{
  flyDocFile_t *pImgFile;

  if(pDoc->opts.verbose >= FLYDOC_VERBOSE_MORE)
    printf("%s\n", szPath);

  pImgFile = FlyAllocZ(sizeof(*pImgFile));
  if(pImgFile)
  {
    pImgFile->szPath = FlyStrClone(szPath);
    if(!pImgFile->szPath)
      FlyFree(pImgFile);
    else
    {
      pDoc->pImgFileList = FlyListAppend(pDoc->pImgFileList, pImgFile);
    }
  }
}

/*!------------------------------------------------------------------------------------------------
  Helper to FLyDocPreProcess(). Fills in pDoc->pImgFileList.

  @param    pDoc      A document state machine initialized with FlyDocInit()
  @param    szPath    `file.jpg` , `../images\*` or `../folder/`
  @return   return TRUE if worked, FALSE to abort
-------------------------------------------------------------------------------------------------*/
bool_t FlyDocProcessImage(const char *szPath, void *pData)
{
  flyDoc_t  *pDoc = pData;

  if(FlyStrPathHasExt(szPath, m_szImageExts))
    FlyDocImgFileListAdd(pDoc, szPath);

  return TRUE;
}

/*!------------------------------------------------------------------------------------------------
  This gets a list of input images as they may be referenced in the source code.

  Fills in pDoc->pImgFileList with all input image files. All, some or none of the images may be
  referenced by markdown text or @logo keyword.

  @param    pDoc      flydoc state
  @param    szPath    `file.jpg` , `../images\*` or `../folder/`
  @return   none
-------------------------------------------------------------------------------------------------*/
void FlyDocPreProcess(flyDoc_t *pDoc, const char *szPath)
{
  bool_t  fExists   = FALSE;
  bool_t  fIsFolder = FALSE;

  // wildcard, let FlyFileListRecurse() process it
  if(strpbrk(szPath, "*?"))
  {
    fExists = TRUE;
    fIsFolder = TRUE;
  }
  else
    fExists = FlyFileExists(szPath, &fIsFolder);

  if(fExists)
  {
    // add any files with proper extension to image list, recursing into subfolders
    if(fIsFolder)
      FlyFileListRecurse(szPath, FLYDOC_MAX_DEPTH, FlyDocProcessImage, pDoc);

    // single file, if it's the right extention, add to image list
    else if(FlyStrPathHasExt(szPath, m_szImageExts))
      FlyDocImgFileListAdd(pDoc, szPath);
  }
}

/*!------------------------------------------------------------------------------------------------
  Fills in the flyDoc_t structture from input files and folders.

  Outputs warnings and errors found in the input files.

  The flydoc state is guaranteed to be valid. If any invalid input, then that input may be ignored.

  @param    pDoc      flydoc state
  @param    szPath    `file.c` , `wildcard/path\*.py` or `../folder/`
  @return   none
-------------------------------------------------------------------------------------------------*/
void FlyDocProcessFolderTree(flyDoc_t *pDoc, const char *szPath)
{
  void         *hList;
  const char   *pszPath;
  unsigned      i;

  if(pDoc->opts.debug)
    printf("--- FlyDocProcessFolderTree(level=%u, path=%s) ---\n", pDoc->level, szPath);

  // szInPath used for recursion into subfolders
  if(pDoc->level == 0)
    *pDoc->szInPath = '\0';

  // single file, process it
  if(FlyFileExistsFile(szPath))
  {
    FlyDocParseFile(pDoc, szPath);
  }

  else
  {
    // create a list from the path (if folder, will add wildcard)
    hList = FlyFileListNewEx(szPath);
    if(!hList)
    {
      FlyDocPrintWarning(pDoc, szWarningInvalidInput, szPath);
      return;
    }

    // traverse folder tree
    else
    {
      for(i = 0; i < FlyFileListLen(hList); ++i)
      {
        pszPath = FlyFileListGetName(hList, i);
        if(FlyStrPathIsFolder(pszPath))
        {
          ++pDoc->level;
          strcpy(pDoc->szInPath, pszPath);
          strcat(pDoc->szInPath, "*");
          FlyDocProcessFolderTree(pDoc, pDoc->szInPath);
          FlyStrPathParent(pDoc->szInPath, PATH_MAX);
          --pDoc->level;
        }
        else
        {
          FlyDocParseFile(pDoc, pszPath);
        }
      }

      FlyFileListFree(hList);
    }
  }
}
