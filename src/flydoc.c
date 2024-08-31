/**************************************************************************************************
  flydoc.c - Create documentation from source code
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

/*!
  @mainpage flydoc    
  @logo ![flydoc](fireflylogo.png "w3-round")
  @version 1.0
  @color w3-blue w3-indigo

  A Tool for Documenting Source Code

  ### Features

  - All comments treated as markdown for rich display in HTML
  - Like markdown, flydoc is minimalist in nature
  - Create developer API documentation from commented source code
  - Works with with most coding languages, C/C++, Java, Python, Rust, Javascript, etc...
  - Supports all Unicode symbols, emoji and languages by using UTF-8 as encoding
  - Control what is documented and what is not included (public APIs vs private functions/methods)
  - Mobile first: HTML looks great on a phone, tablet or desktop
  - Adjust colors and images for project/company customized look to HTML
  - Output options include:
    1. A set of static HTML/CSS files with links and images
    2. A set of markdown files
    3. A single markdown file
  - Predictable links for easy references
  - Warnings are in standard `file:line:col: warning: text` format for easy parsing
*/

/*!
  @defgroup flydoc_main   The Main Entry - Handles Command-Line Interface
*/

/*!-------------------------------------------------------------------------------------------------
  Create a folder if doesn't already exist. Return full path if worked.

  @param  szFullPath    full path to be received (must be size PATH_MAX)
  @param  szPath        partial path to folder to create if doesn't already exist
  @return TRUE if worked, FALSE if couldn't create folder or szPath was file
-------------------------------------------------------------------------------------------------*/
bool_t FlyDocCreateFolder(flyDoc_t *pDoc, const char *szPath)
{
  sFlyFileInfo_t  info;
  bool_t          fWorked = FALSE;

  // does the folder already exist?
  for(int i = 0; i < 2; ++i)
  {
    // does the folder already exist?
    memset(&info, 0, sizeof(info));
    if(FlyFileInfoGet(&info, szPath))
    {
      if(info.fIsDir)
        fWorked = TRUE;
      break;
    }

    // don't try to mkdir() twice
    if(i == 1)
      break;

    // folder doesn't exist, try to make it
    if(mkdir(szPath, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) < 0)
      break;
  }

  return fWorked;
}

/*!------------------------------------------------------------------------------------------------
  Determine total number of flydoc objects (modules, functions, documents, etc...)

  @param    pDoc    ptr to the full document structure
  @param    fFull   if FALSE, prints pointers and single lines only
  @return   none
-------------------------------------------------------------------------------------------------*/
unsigned FlyDocNumObjects(const flyDoc_t *pDoc)
{
  unsigned total = 0;

  if(pDoc->pMainPage)
    ++total;

  total += pDoc->nModules + pDoc->nFunctions + pDoc->nClasses + pDoc->nMethods +
           pDoc->nExamples + pDoc->nDocuments;
  return total;
}

/*!------------------------------------------------------------------------------------------------
  Check that memory was allocated. Asserts with stack trace if memory ptr is NULL.

  Since this is a desktop program, it is extremely unlikely it will run out of memory.

  @param    pDoc    ptr to the full document structure
  @param    pMem    ptr to allocated memory (which, if failed, is NULL)
  @return   none
-------------------------------------------------------------------------------------------------*/
void FlyDocAllocCheck(void *pMem)
{
  if(pMem == NULL)
    FlyDocAssertMem();
}

/*!------------------------------------------------------------------------------------------------
  Same as FlyAlloc, but asserts with stack trace if failed to allocate memory.

  @param    n       # of bytes to allocate from heap
  @return   ptr to allocated memory
-------------------------------------------------------------------------------------------------*/
void * FlyDocAlloc(size_t n)
{
  void  *pMem = FlyAlloc(n);
  FlyDocAllocCheck(pMem);
  return pMem;
}

/*!------------------------------------------------------------------------------------------------
  Is this a flytdoc object?

  @param    pDoc    a flyDoc_t object
  @return   TRUE if is a flyDoc_t object, FALSE if not
-------------------------------------------------------------------------------------------------*/
bool_t FlyDocIsDoc(const flyDoc_t *pDoc)
{
  return (pDoc && (pDoc->sanchk == SANCHK_DOC)) ? TRUE : FALSE;
}

/*!------------------------------------------------------------------------------------------------
  Initialize the flydoc object. Verifies the out file/folder in pOpts exists or can be created.

  If pOpts.fVerbose is set, then the warnings will be produced on stderr

  @param    pDoc    a flyDoc_t object.
  @param    pOpts   ptr to cmdline options
  @return   TRUE if worked, FALSE if failed
-------------------------------------------------------------------------------------------------*/
void FlyDocInit(flyDoc_t *pDoc, const flyDocOpts_t *pOpts)
{
  memset(pDoc, 0, sizeof(*pDoc));
  pDoc->sanchk    = SANCHK_DOC;
  pDoc->opts      = *pOpts;
  if(!pDoc->opts.szExts)
    pDoc->opts.szExts = SZ_FLY_DOC_EXTS;
}

/*!-------------------------------------------------------------------------------------------------
  Get the styles for the HTML page from the section data.

  Handle the heirarchy of local page first, main page next, then default (which may be NULL).

  1. prefer pSection colors/styles
  2. if not, use mainpage colors/styles
  3. if no mainpage, use default colors/styles

  @param    pDoc      Contains the pMainPage
  @param    pSection  The current section to get style from
  @param    pStyle    Return value, styles for color, font, images, etc...
  @return   none
-------------------------------------------------------------------------------------------------*/
void FlyDocStyleGet(flyDoc_t *pDoc, flyDocSection_t *pSection, flyDocStyle_t *pStyle)
{
  static const char szW3HtlmColorBar[]          = FLYDOC_DEF_BAR_COLOR;
  static const char szW3HtlmColorTitle[]        = FLYDOC_DEF_TITLE_COLOR;
  static const char szW3HtlmColorHeadingBar[]   = FLYDOC_DEF_HBAR_COLOR;
  static const char szEmpty[]                   = "";
  static const char szHomeImgRef[]              = "![Home](flydoc_home.png \"w3-round\")";

  flyDocMainPage_t *pMainPage    = pDoc->pMainPage;
  bool_t            fIsMainPage;

  FlyAssert(pDoc && pSection && pStyle);

  fIsMainPage = (pDoc->pMainPage && pSection == &pDoc->pMainPage->section) ? TRUE : FALSE;

  // determine side bar color
  pStyle->szBarColor = pSection->szBarColor;
  if(pSection->szBarColor == NULL && pMainPage)
    pStyle->szBarColor = pMainPage->section.szBarColor;
  if(pStyle->szBarColor == NULL)
    pStyle->szBarColor = (char *)szW3HtlmColorBar;

  // determine title bar color
  pStyle->szTitleColor = pSection->szTitleColor;
  pStyle->szHeadingColor = pSection->szHeadingColor;
  if(pSection->szTitleColor == NULL && pMainPage)
    pStyle->szTitleColor = pMainPage->section.szTitleColor;
  if(pStyle->szTitleColor == NULL)
    pStyle->szTitleColor = (char *)szW3HtlmColorTitle;

  // determine heading color
  if(fIsMainPage)
    pStyle->szHeadingColor = pSection->szHeadingColor;
  if(pStyle->szHeadingColor == NULL && pMainPage)
    pStyle->szHeadingColor = pMainPage->section.szHeadingColor;
  if(pStyle->szHeadingColor == NULL)
    pStyle->szHeadingColor = (char *)szW3HtlmColorHeadingBar; 

  // OK if szFontBody or szFontHeadings is NULL
  pStyle->szFontBody = pSection->szFontBody;
  if(pSection->szFontBody == NULL && pDoc->pMainPage)
    pStyle->szFontBody = pDoc->pMainPage->section.szFontBody;
  pStyle->szFontHeadings = pSection->szFontHeadings;
  if(pSection->szFontHeadings == NULL && pDoc->pMainPage)
    pStyle->szFontHeadings = pDoc->pMainPage->section.szFontHeadings;

  // determine logo image, e.g. ![alt text](logo.png), Ok to be NULL
  pStyle->szLogo = pSection->szLogo;
  if(pSection->szLogo == NULL && pDoc->pMainPage)
    pStyle->szLogo = pDoc->pMainPage->section.szLogo;
  if(pStyle->szLogo == NULL)
  {
    pDoc->fNeedImgHome = TRUE;    
    pStyle->szLogo = (char *)szHomeImgRef;
  }

  // determine version string, empty string if no version
  pStyle->szVersion = pSection->szVersion;
  if(pSection->szVersion == NULL && pDoc->pMainPage)
    pStyle->szVersion = pDoc->pMainPage->section.szVersion;
  if(pStyle->szVersion == NULL)
    pStyle->szVersion = (char *)szEmpty;
}

/*!------------------------------------------------------------------------------------------------
  Copy any referenced images to ouput folder.

  Uses pDoc->szOutPath, pImgFileList

  @param    pDoc    Filled out flyDoc_t object
  @return   none
-------------------------------------------------------------------------------------------------*/
void FlyDocCopyReferencedImages(flyDoc_t *pDoc)
{
  flyDocFile_t *pImgFile = pDoc->pImgFileList;

  while(pImgFile)
  {
    if(pImgFile->fReferenced)
    {
      FlyStrZCpy(pDoc->szPath, pDoc->opts.szOut, sizeof(pDoc->szPath));
      FlyStrPathAppend(pDoc->szPath, FlyStrPathNameOnly(pImgFile->szPath), sizeof(pDoc->szPath));
      if(pDoc->opts.verbose >= FLYDOC_DEBUG_MORE)
        printf("  Copying %s => %s\n", pImgFile->szPath, pDoc->szPath);
      if(!FlyFileCopy(pDoc->szPath, pImgFile->szPath))
        FlyDocAssertMem();
    }
    pImgFile = pImgFile->pNext;
  }
}

/*!------------------------------------------------------------------------------------------------
  main entry to flydoc

  @param    argc
  @param    argv
  @return   0 if worked, 1 if failure
-------------------------------------------------------------------------------------------------*/
int main(int argc, const char *argv[])
{
  flyDocOpts_t        opts;
  const flyCliOpt_t   cliOpts[] =
  {
    { "-n",           &opts.fNoBuild,   FLYCLI_BOOL },
    { "-o",           &opts.szOut,      FLYCLI_STRING },
    { "-s",           &opts.fSort,      FLYCLI_BOOL },
    { "-v",           &opts.verbose,    FLYCLI_INT },
    { "--debug",      &opts.debug,      FLYCLI_INT },     // hidden option
    { "--exts",       &opts.szExts,     FLYCLI_STRING },
    { "--local",      &opts.fLocal,     FLYCLI_BOOL },
    { "--markdown",   &opts.fMarkdown,  FLYCLI_BOOL },
    { "--noindex",    &opts.fNoIndex,   FLYCLI_BOOL },
    { "--slug",       &opts.szSlug,     FLYCLI_STRING },  // make a slug from a string
    { "--user-guide", &opts.fUserGuide, FLYCLI_BOOL },
  };
  const flyCli_t cli =
  {
    .pArgc      = &argc,
    .argv       = argv,
    .nOpts      = NumElements(cliOpts),
    .pOpts      = cliOpts,
    .szVersion  = "flydoc v" FLYDOC_VER_STR,
    .szHelp     = "Usage = flydoc [-n] [-o out/] [-s] [-v] [--combine] [--exts .c.js] [--local] [--markdown] [--noindex] in...\n"
    "\n"
    "Options:\n"
    "-n               Parse inputs only, no output, useful to check for warnings\n"
    "-o               Output folder/\n"
    "-s               Sort modules/functions/classes/methods: -s- (off), -s (on: default)\n"
    "-v[=#]           Verbosity: -v- (none), -v=1 (some), -v=2 (more: default)\n"
    "--exts           List of file exts to search. Default: \".c.c++.cc.cpp.cxx.cs.go.java.js.py.rs.swift.ts\"\n"
    "--local          Create local w3.css file rather than remote link to w3.css\n"
    "--markdown       Create a single combine markdown file rather than HTML pages\n"
    "--noindex        Don't create index.html (mainpage). Allows for custom main page\n"
    "--slug \"str\"     Print local reference id (slug) from a string\n"
    "--user-guide     Print flydoc user guide to the screen\n"
    "in...            Input files and folders\n"
  };
  flyDoc_t  flyDoc;
  int       nArgs;
  int       i;
  bool_t    fWorked = TRUE;

  memset(&flyDoc, 0, sizeof(flyDoc));
  memset(&opts, 0, sizeof(opts));
  opts.verbose = FLYDOC_VERBOSE_MORE;
  opts.fSort   = TRUE;
  if(FlyCliParse(&cli) != FLYCLI_ERR_NONE)
    exit(1);

  // print the manual to the screen
  if(opts.fUserGuide)
  {
    puts(szFlyDocManual);
    exit(0);
  }
  if(opts.szSlug)
  {
    FlyDocPrintSlug(opts.szSlug);
    exit(0);
  }

  if(opts.debug)
    FlyDocPrintBanner("flydoc v" FLYDOC_VER_STR);
  else if(opts.verbose)
    printf("flydoc v" FLYDOC_VER_STR "\n");
  nArgs = FlyCliNumArgs(&cli);
  if(nArgs < 2)
  {
    printf("No input files or folders. Try flydoc --help\n");
    exit(1);
  }

  // initialize document structure
  FlyDocInit(&flyDoc, &opts);

  // must specifiy output file or folder
  if(!opts.fNoBuild && opts.szOut == NULL)
  {
    printf("No output folder specified, use -o folder/\n");
    exit(1);
  }

  // display options
  if(opts.debug)
  {
    printf("\nflydoc options: -v=%u --markdown=%u --exts=%s --debug=%u, -o=%s\n\n",
      flyDoc.opts.verbose, flyDoc.opts.fMarkdown, flyDoc.opts.szExts, flyDoc.opts.debug, flyDoc.opts.szOut);
  }

  // parse the input files
  if((flyDoc.opts.verbose >= FLYDOC_VERBOSE_MORE) || flyDoc.opts.debug)
    printf("\nProcessing file(s)...\n");

  // collect all images into an array of files (flyDoc.apImagesFiles)
  for(i = 1; i < nArgs; ++i)
  {
    flyDoc.level = 0;
    FlyDocPreProcess(&flyDoc, FlyCliArg(&cli, i));
  }

  if(opts.debug >= 12)
  {
    FlyDocPrintDoc(&flyDoc, opts.debug);
    exit(1);
  }

  // parse all inputs files into the flyDoc_t structure
  for(i = 1; i < nArgs; ++i)
  {
    flyDoc.level = 0;
    FlyDocProcessFolderTree(&flyDoc, FlyCliArg(&cli, i));
  }

  // calculate statistics
  FlyDocStatsUpdate(&flyDoc);

  // print out internal structures
  if(opts.debug)
    FlyDocPrintDoc(&flyDoc, opts.debug);

  if(FlyDocNumObjects(&flyDoc) == 0)
  {
    FlyDocPrintWarning(&flyDoc, szWarningNoObjects, NULL);
  }

  else if(!flyDoc.opts.fNoBuild)
  {
    if((flyDoc.opts.verbose >= FLYDOC_VERBOSE_MORE) || flyDoc.opts.debug)
      printf("\nCreating file(s)...\n");
    if(flyDoc.opts.fMarkdown)
    {
      if(!FlyDocWriteMarkdown(&flyDoc))
        fWorked = FALSE;
    }
    else
    {
      if(!FlyDocWriteHtml(&flyDoc))
        fWorked = FALSE;
    }

    // copy any locally referenced images to out folder
    if(fWorked)
      FlyDocCopyReferencedImages(&flyDoc);
    FlyDocStatsUpdate(&flyDoc);
  }

  // print # of modules, classes, functions, examples, etc...
  if(fWorked && flyDoc.opts.verbose && !(opts.debug && opts.fNoBuild))
    FlyDocPrintStats(&flyDoc);

  return flyDoc.nWarnings ? 1 : 0;
}
