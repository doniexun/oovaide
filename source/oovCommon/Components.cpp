/*
 * Components.cpp
 *
 *  Created on: Jan 4, 2014
 *  \copyright 2014 DCBlaha.  Distributed under the GPL.
 */

#include "Components.h"
#include "FilePath.h"
#include "Project.h"
#include "OovString.h"  // For split
#include "OovError.h"
#include <algorithm>


/*
FilePaths getAsmSourceExtensions()
    {
    .S
    .asm
    }
*/

/*
FilePaths getJavaSourceExtension()
    {
    .java
    .class - compiled java
    .dpj - dependency list
    .jar archived classes (similar to lib)
    }
*/

FilePaths getCppHeaderExtensions()
    {
    FilePaths strs;
    strs.push_back(FilePath("h", FP_Ext));
    strs.push_back(FilePath("hh", FP_Ext));
    strs.push_back(FilePath("hpp", FP_Ext));
    strs.push_back(FilePath("hxx", FP_Ext));
    strs.push_back(FilePath("inc", FP_Ext));
    return strs;
    }

FilePaths getCppSourceExtensions()
    {
    FilePaths strs;
    strs.push_back(FilePath("c", FP_Ext));
    strs.push_back(FilePath("cc", FP_Ext));
    strs.push_back(FilePath("c++", FP_Ext));
    strs.push_back(FilePath("cpp", FP_Ext));
    strs.push_back(FilePath("cxx", FP_Ext));
    return strs;
    }

FilePaths getLibExtensions()
    {
    FilePaths strs;
    strs.push_back(FilePath("a", FP_Ext));
    strs.push_back(FilePath("lib", FP_Ext));
    return strs;
    }

FilePaths getJavaSourceExtensions()
    {
    FilePaths strs;
    strs.push_back(FilePath("java", FP_Ext));
    return strs;
    }

bool isJavaSource(OovStringRef const file)
    { return(FilePathAnyExtensionMatch(getJavaSourceExtensions(), file)); }

bool isCppHeader(OovStringRef const file)
    { return(FilePathAnyExtensionMatch(getCppHeaderExtensions(), file)); }

bool isCppSource(OovStringRef const file)
    { return(FilePathAnyExtensionMatch(getCppSourceExtensions(), file)); }

bool isLibrary(OovStringRef const file)
    { return(FilePathAnyExtensionMatch(getLibExtensions(), file)); }

OovStatusReturn ComponentTypesFile::read()
    {
    mCompTypesFile.setFilename(Project::getComponentTypesFilePath());
    mCompSourceListFile.setFilename(Project::getComponentSourceListFilePath());
    OovStatus status = mCompSourceListFile.readFile();
    // For new projects, the files are optional.
    /// @todo - should detect the difference between missing files and disk errors.
    if(status.needReport())
        {
        status.clearError();
        }
    status = mCompTypesFile.readFile();
    if(status.needReport())
        {
        status.clearError();
        }
    return status;
    }

OovStatusReturn ComponentTypesFile::readTypesOnly(OovStringRef const fn)
    {
    mCompTypesFile.setFilename(fn);
    return mCompTypesFile.readFile();
    }

OovStringVec ComponentTypesFile::getComponentNames(bool definedComponentsOnly) const
    {
    OovStringVec compNames = CompoundValueRef::parseString(
            mCompTypesFile.getValue("Components"));
    if(definedComponentsOnly)
        {
        compNames.erase(std::remove_if(compNames.begin(), compNames.end(),
            [this](OovString &name)
            { return(getComponentType(name) == CT_Unknown); }), compNames.end());
        }
    return compNames;
    }

std::string ComponentTypesFile::getComponentChildName(std::string const &compName)
    {
    OovString child = compName;
    size_t pos = child.rfind('/');
    if(pos != OovString::npos)
        {
        child.erase(0, pos+1);
        }
    return child;
    }

std::string ComponentTypesFile::getComponentParentName(std::string const &compName)
    {
    OovString parent = compName;
    size_t pos = parent.rfind('/');
    if(pos != OovString::npos)
        {
        parent.erase(pos);
        }
    return parent;
    }

bool ComponentTypesFile::anyComponentsDefined() const
    {
    auto const &names = getComponentNames();
    return std::any_of(names.begin(), names.end(),
            [=](std::string const &name)
            {return(getComponentType(name) != CT_Unknown);} );
    }

enum ComponentTypesFile::eCompTypes ComponentTypesFile::getComponentType(
        OovStringRef const compName) const
    {
    OovString tag = getCompTagName(compName, "type");
    OovString typeStr = mCompTypesFile.getValue(tag);
    return getComponentTypeFromTypeName(typeStr);
    }

void ComponentTypesFile::coerceParentComponents(OovStringRef const compName)
    {
    OovString name = compName;
    while(1)
        {
        size_t pos = name.rfind('/');
        if(pos != std::string::npos)
            {
            name.erase(pos);
            }
        if(compName != name.getStr())
            {
            setComponentType(name, CT_Unknown);
            }
        if(pos == std::string::npos)
            {
            break;
            }
        }
    }

OovStatusReturn ComponentTypesFile::writeFile()
    {
    OovStatus status = mCompTypesFile.writeFile();
    if(status.needReport())
        {
        OovString str = "Unable to write component types file: ";
        str += mCompTypesFile.getFilename();
        status.report(ET_Error, str);
        }
    if(status.ok())
        {
        status = mCompSourceListFile.writeFile();
        if(status.needReport())
            {
            OovString str = "Unable to write source list file: ";
            str += mCompSourceListFile.getFilename();
            status.report(ET_Error, str);
            }
        }
    return status;
    }

OovStatusReturn ComponentTypesFile::writeTypesOnly(OovStringRef const fn)
    {
    mCompTypesFile.setFilename(fn);
    OovStatus status = mCompTypesFile.writeFile();
    if(status.needReport())
        {
        OovString str = "Unable to write source list file: ";
        str += mCompTypesFile.getFilename();
        OovError::report(ET_Error, str);
        }
    return status;
    }


// This should match "Parameter"="Parameter/PLib", but not match "Comm"!="CommSim"
static bool compareComponentNames(std::string const &parentName, std::string const &childName)
    {
    bool child = false;
    if(childName.length() > parentName.length())
        {
        if(childName[parentName.length()] == '/')
            {
            child = (parentName.compare(0, parentName.length(), childName, 0,
                    parentName.length()) == 0);
            }
        }
    return((parentName == childName) || child);
    }

void ComponentTypesFile::coerceChildComponents(OovStringRef const compName)
    {
    OovStringVec names = getComponentNames();
    OovString parentName = compName;
    for(auto const &name : names)
        {
        if(name != parentName)
            {
            if(compareComponentNames(parentName, name))
                {
                setComponentType(name, CT_Unknown);
                }
            }
        }
    }

void ComponentTypesFile::setComponentType(OovStringRef const compName, eCompTypes ct)
    {
    OovString tag = getCompTagName(compName, "type");
    OovStringRef const value = getComponentTypeAsFileValue(ct);
    mCompTypesFile.setNameValue(tag, value);
    }

void ComponentTypesFile::setComponentType(OovStringRef const compName,
    OovStringRef const typeName)
    {
    eCompTypes newType = getComponentTypeFromTypeName(typeName);
    if(newType != CT_Unknown && newType != getComponentType(compName))
        {
        coerceParentComponents(compName);
        coerceChildComponents(compName);
        }
    setComponentType(compName, getComponentTypeFromTypeName(typeName));
    }

enum ComponentTypesFile::eCompTypes ComponentTypesFile::getComponentTypeFromTypeName(
        OovStringRef const compTypeName)
    {
    eCompTypes ct = CT_Unknown;
    if(compTypeName.numBytes() != 0)
        {
        if(compTypeName[0] == 'P')
            ct = CT_Program;
        else if(compTypeName[0] == 'U')
            ct = CT_Unknown;
        else if(compTypeName[1] == 't')
            ct = CT_StaticLib;
        else if(compTypeName[1] == 'h')
            ct = CT_SharedLib;
        }
    return ct;
    }

OovStringRef const ComponentTypesFile::getLongComponentTypeName(eCompTypes ct)
    {
    char const *p = NULL;
    switch(ct)
        {
        case CT_Unknown:        p = "Undefined/Not Applicable";         break;
        case CT_StaticLib:      p = "Static/Compile-time Library";      break;
        case CT_SharedLib:      p = "Shared/Run-time Library";          break;
        case CT_Program:        p = "Program/Executable";               break;
        }
    return p;
    }

OovStringRef const ComponentTypesFile::getComponentTypeAsFileValue(eCompTypes ct)
    {
    char const *p = NULL;
    switch(ct)
        {
        case CT_Unknown:        p = "Unknown";          break;
        case CT_StaticLib:      p = "StaticLib";        break;
        case CT_SharedLib:      p = "SharedLib";        break;
        case CT_Program:        p = "Program";          break;
        }
    return p;
    }

OovString ComponentTypesFile::getCompTagName(OovStringRef const compName,
    OovStringRef const tag)
    {
    OovString tagName = "Comp-";
    tagName += tag;
    tagName += "-";
    tagName += compName;
    return tagName;
    }

OovStringRef ComponentTypesFile::getCompFileTypeTagName(CompFileTypes cft)
    {
    char const *name = nullptr;
    switch(cft)
        {
        case CFT_CppSource:     name = "cppSrc";   break;
        case CFT_CppInclude:    name = "cppInc";   break;
        case CFT_JavaSource:    name = "java";   break;
        }
    return(name);
    }

void ComponentTypesFile::setComponentFiles(CompFileTypes cft,
    OovStringRef const compName, OovStringSet const &srcs)
    {
    CompoundValue objArgs;
    for(const auto &src : srcs)
        {
        objArgs.addArg(src);
        }
    OovString tag = getCompTagName(compName, getCompFileTypeTagName(cft));
    mCompSourceListFile.setNameValue(tag, objArgs.getAsString());
    }

OovStringVec ComponentTypesFile::getComponentFiles(CompFileTypes cft,
        OovStringRef const compName, bool getNested) const
    {
    return getComponentFiles(compName, getCompFileTypeTagName(cft), getNested);
    }

OovStringVec ComponentTypesFile::getComponentFiles(OovStringRef const compName,
        OovStringRef const tagStr, bool getNested) const
    {
    OovStringVec files;
    OovStringVec names = getComponentNames();
    OovString parentName = compName;
    for(auto const &name : names)
        {
        bool match = false;
        if(getNested)
            {
            match = compareComponentNames(parentName, name);
            }
        else
            {
            match = (parentName == name);
            }
        if(match)
            {
            OovString tag = ComponentTypesFile::getCompTagName(name, tagStr);
            OovString val = mCompSourceListFile.getValue(tag);
            OovStringVec newFiles = CompoundValueRef::parseString(val);
            files.insert(files.end(), newFiles.begin(), newFiles.end());
            }
        }
    return files;
    }

OovString ComponentTypesFile::getComponentBuildArgs(
        OovStringRef const compName) const
    {
    OovString tag = ComponentTypesFile::getCompTagName(compName, "args");
    return mCompTypesFile.getValue(tag);
    }

void ComponentTypesFile::setComponentBuildArgs(OovStringRef const compName,
        OovStringRef const args)
    {
    OovString tag = ComponentTypesFile::getCompTagName(compName, "args");
    mCompTypesFile.setNameValue(tag, args);
    }

OovString ComponentTypesFile::getComponentAbsolutePath(
        OovStringRef const compName) const
    {
    OovString path;
    /// @todo - fix: this doesn't work if the component has no immediate files.
    OovStringVec src = getComponentFiles(CFT_CppSource, compName, true);
    if(src.size() == 0)
        {
        src = getComponentFiles(CFT_CppInclude, compName, true);
        }
    if(src.size() > 0)
        {
        FilePath fp;
        fp.getAbsolutePath(src[0], FP_File);
        fp.discardFilename();
        path = fp;
        }
    return path;
    }


//////////////

void ComponentsFile::read(OovStringRef const fn)
    {
    setFilename(fn);
    OovStatus status = readFile();
    if(status.needReport())
        {
        // The components file is normally optional.
        status.reported();
/*
        OovString err = "Unable to read components file ";
        err += fn;
        status.report(ET_Error, err);
*/
        }
    }

void ComponentsFile::parseProjRefs(OovStringRef const arg, OovString &rootDir,
        OovStringVec &excludes)
    {
    excludes.clear();
    OovStringVec tokens = StringSplit(arg, '!');
    if(rootDir.size() == 0)
        rootDir = tokens[0];
    if(tokens.size() > 1)
        {
        excludes.resize(tokens.size()-1);
        std::copy(tokens.begin()+1, tokens.end(), excludes.begin());
        }
    }

// DEAD CODE
/*
OovString ComponentsFile::getProjectIncludeDirsStr() const
    {
    return getValue("Components-init-proj-incs");
    }
*/

// DEAD CODE
/*
OovStringVec ComponentsFile::getAbsoluteIncludeDirs() const
    {
    OovString val = getProjectIncludeDirsStr();
    OovStringVec incs = CompoundValueRef::parseString(val);
    std::for_each(incs.begin(), incs.end(), [](std::string &fn)
        {
        FilePath fp;
        fp.getAbsolutePath(fn, FP_Dir);
        fn = fp;
        });
    return incs;
    }
*/
