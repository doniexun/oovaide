/*
 * Options.cpp
 *
 *  Created on: Jun 27, 2013
 *  \copyright 2013 DCBlaha.  Distributed under the GPL.
 */

#include "Options.h"
#include <stdlib.h>	// for getenv
#include "FilePath.h"
#include "Project.h"
#include "OovString.h"


BuildOptions gBuildOptions;
GuiOptions gGuiOptions;

#ifndef __linux__
/*
// Return is empty if search string not found
static std::string getPathSegment(const std::string &path, char const * const srch)
    {
    OovString lowerPath;
    lowerPath.setLowerCase(path.c_str());
    std::string seg;
    size_t pos = lowerPath.find(srch);
    if(pos != std::string::npos)
	{
	std::string arg = "-ER";
	size_t startPos = path.rfind(';', pos);
	if(startPos != std::string::npos)
	    startPos++;
	else
	    startPos = 0;
	size_t endPos = path.find(';', pos);
	if(endPos == std::string::npos)
	    endPos = path.length();
	seg = path.substr(startPos, endPos-startPos);
	}
    return seg;
    }

static bool addExternalReference(const std::string &path, char const * const srch,
	CompoundValue &args)
    {
    std::string seg = getPathSegment(path, srch);
    size_t binPos = seg.find("bin", seg.length()-3);
    if(binPos != std::string::npos)
	{
	seg.resize(binPos);
	removePathSep(seg, binPos-1);
	}
    if(seg.size() > 0)
	{
	std::string arg = "-ER";
	arg += seg;
	args.addArg(arg.c_str());
	}
    return seg.size() > 0;
    }
*/
#endif

static void addCLangArgs(CompoundValue &cv)
    {
    cv.addArg("-x");
    cv.addArg("c++");
    cv.addArg("-std=c++11");
    }

#ifdef __linux__
std::string makeExeFilename(char const * const rootFn)
    { return rootFn; }
#else
std::string makeExeFilename(char const * const rootFn)
    { return (std::string(rootFn) + ".exe"); }
#endif

static void setBuildConfigurationPaths(NameValueFile &file,
	char const * const buildConfig, char const * const extraArgs, bool useclang)
    {
    std::string optStr = makeBuildConfigArgName(OptExtraBuildArgs, buildConfig);
    file.setNameValue(optStr.c_str(), extraArgs);

    if(std::string(buildConfig).compare(BuildConfigAnalysis) != 0)
	{
	// Assume the archiver is installed and on path.
	// llvm-ar gives link error.
	// setNameValue(makeExeFilename("llvm-ar"));
	optStr = makeBuildConfigArgName(OptToolLibPath, buildConfig);
	file.setNameValue(optStr.c_str(), makeExeFilename("ar").c_str());

	// llvm-nm gives bad file error on Windows, and has no output on Linux.
	// mPathObjSymbol = "makeExeFilename(llvm-nm)";
	optStr = makeBuildConfigArgName(OptToolObjSymbolPath, buildConfig);
	file.setNameValue(optStr.c_str(), makeExeFilename("nm").c_str());

	std::string compiler;
	if(useclang)
	    compiler = makeExeFilename("clang++");
	else
	    compiler = makeExeFilename("g++");
	optStr = makeBuildConfigArgName(OptToolCompilePath, buildConfig);
	file.setNameValue(optStr.c_str(), compiler.c_str());
	}
    }

void BuildOptions::setDefaultOptions()
    {
    CompoundValue baseArgs;
    CompoundValue extraCppDocArgs;
    CompoundValue extraCppRlsArgs;
    CompoundValue extraCppDbgArgs;

    baseArgs.addArg("-c");
    bool useCLangBuild = false;
#ifdef __linux__
//    baseArgs.addArg("-ER/usr/include/gtk-3.0");
//    baseArgs.addArg("-ER/usr/lib/x86_64-linux-gnu/glib-2.0/include");
    if(fileExists("/usr/bin/clang++"))
	useCLangBuild = true;
#else
    std::string path = getenv("PATH");
    if(path.find("clang") != std::string::npos)
	{
	useCLangBuild = true;
	}
/*
    // On Windows, GTK includes gmodule, glib, etc., so no reason to get mingw.
    bool success = addExternalReference(path, "gtk", baseArgs);
    if(!success)
	{
	addExternalReference(path, "mingw", baseArgs);
	}
    addExternalReference(path, "qt", baseArgs);
*/
#endif
    // The GNU compiler cannot have these, but the clang compiler requires them.
    // CLang 3.2 does not work on Windows well, so GNU is used on Windows for
    // building, but CLang is used for XMI parsing.
    // These are important for xmi parsing to show template relations
    if(useCLangBuild)
	{
	addCLangArgs(baseArgs);
	}
    else
	{
	addCLangArgs(extraCppDocArgs);
	extraCppDbgArgs.addArg("-std=c++0x");
	extraCppRlsArgs.addArg("-std=c++0x");
	}
    extraCppDbgArgs.addArg("-O0");
    extraCppDbgArgs.addArg("-g3");

    extraCppRlsArgs.addArg("-O3");

    setBuildConfigurationPaths(*this, BuildConfigAnalysis,
	    extraCppDocArgs.getAsString().c_str(), useCLangBuild);
    setBuildConfigurationPaths(*this, BuildConfigDebug,
	    extraCppDbgArgs.getAsString().c_str(), useCLangBuild);
    setBuildConfigurationPaths(*this, BuildConfigRelease,
	    extraCppRlsArgs.getAsString().c_str(), useCLangBuild);

    setNameValue(OptBaseArgs, baseArgs.getAsString().c_str());

    }

void GuiOptions::setDefaultOptions()
    {
    setNameValueBool(OptGuiShowAttributes, true);
    setNameValueBool(OptGuiShowOperations, true);
    setNameValueBool(OptGuiShowOperParams, true);
    setNameValueBool(OptGuiShowAttrTypes, true);
    setNameValueBool(OptGuiShowOperTypes, true);
    setNameValueBool(OptGuiShowPackageName, true);
    setNameValueBool(OptGuiShowOovSymbols, true);
    setNameValueBool(OptGuiShowOperParamRelations, true);
    setNameValueBool(OptGuiShowOperBodyVarRelations, true);
    /*
    #ifdef __linux__
        setNameValue(OptEditorPath, "/usr/bin/gedit");
    #else
        setNameValue(OptEditorPath, "\\Windows\\notepad.exe");
    #endif
    */
    #ifdef __linux__
        setNameValue(OptGuiEditorPath, "./oovEdit");
    #else
        setNameValue(OptGuiEditorPath, "oovEdit.exe");
    #endif
        setNameValue(OptGuiEditorLineArg, "+");
    }
