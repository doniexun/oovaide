// oovCMaker.h
// Created on: Jan 23, 2014
// \copyright 2014 DCBlaha.  Distributed under the GPL.
#include "Version.h"
#include "Project.h"
#include "oovCMaker.h"
#include "OovString.h"
#include <ctype.h>
#include <algorithm>
#include <stdio.h>


static bool compareNoCase(const std::string& first, const std::string& second)
    {
    OovString str1;
    OovString str2;
    str1.setLowerCase(first);
    str2.setLowerCase(second);
    return(str1 < str2);
    }


CMaker::CMaker(OovStringRef const projName, bool verbose):
    mProjectName(projName), mCompTypes(mProject), mBuildPkgs(false), mVerbose(verbose)
    {
    }

OovStatusReturn CMaker::writeFile(OovStringRef const destName, OovStringRef const str)
    {
    OovStatus status(true, SC_File);
    if(str.numBytes() > 0)
        {
        File file;
        status = file.open(destName, "w");
        if(status.ok())
            {
            status = file.putString("# Generated by oovCMaker\n");
            if(status.ok())
                {
                status = file.putString(str);
                }
            }
        }
    if(!status.ok())
        {
        OovString errStr = "Unable to write file ";
        errStr += destName;
        status.report(ET_Error, errStr);
        }
    return status;
    }

void CMaker::makeDefineName(OovStringRef const pkgName, OovString &defName)
    {
    OovString shortName;
    for(const char *p = pkgName; *p!='\0'; p++)
        {
        if(isalpha(*p))
            shortName += *p;
        }
    defName.setUpperCase(shortName);
    }

/// For any component that has subdirectories "xx/yy", replace the
/// slash with a dash.
static std::string makeIdentifierFromComponentName(std::string const &compName)
    {
    std::string fixedCompName = compName;
    std::replace(fixedCompName.begin(), fixedCompName.end(), '/', '-');
    return fixedCompName;
    }

static std::string makeRelativeIdentifierFromComponentName(OovStringRef const compName,
        std::string const &str)
    {
    std::string dir = str;
    int compNameLen = OovString(compName).length();
    if(dir.compare(0, compNameLen, compName) == 0)
        {
        dir.erase(0, compNameLen+1);
        }
    else
        {
        size_t depth = std::count(dir.begin(), dir.end(), '/');
        for(size_t i=0; i<depth; i++)
            {
            dir.insert(0, "../");
            }
        }
    return makeIdentifierFromComponentName(dir);
    }


void CMaker::addPackageDefines(OovStringRef const pkgName, std::string &str)
    {
    OovString pkgDefName;
    makeDefineName(pkgName, pkgDefName);
    str += std::string("# ") + pkgDefName + "\n";
    str += std::string("pkg_check_modules(") + pkgDefName + " REQUIRED " +
            pkgName.getStr() + ")\n";
    str += std::string("include_directories(${") + pkgDefName + "_INCLUDE_DIRS})\n";
    str += std::string("link_directories(${") + pkgDefName + "_LIBRARY_DIRS})\n";
    str += std::string("add_definitions(${") + pkgDefName + "_CFLAGS_OTHER})\n";
    }

OovStatusReturn CMaker::makeTopMakelistsFile(OovStringRef const destName)
    {
    std::string str;

    str += "cmake_minimum_required(VERSION 2.8)\n";
    str += std::string("project(") + mProjectName + ")\n";
    str += "INCLUDE(FindPkgConfig)\n\n";

    OovString projNameUpper;
    projNameUpper.setUpperCase(mProjectName);
    str += std::string("set(") + projNameUpper + "_MAJOR_VERION 0)\n";
    str += std::string("set(") + projNameUpper + "_MINOR_VERION 1)\n";
    str += std::string("set(") + projNameUpper + "_PATCH_VERION 0)\n";
    str += std::string("set(") + projNameUpper + "_VERION\n";
    str += std::string("${") + projNameUpper + "_MAJOR_VERSION}.${";
    str += projNameUpper + "_MINOR_VERSION}.${";
    str += projNameUpper + "_PATCH_VERSION})\n\n";

    str += "# Offer the user the choice of overriding the installation directories\n";
    str += "set(INSTALL_LIB_DIR lib CACHE PATH \"Installation directory for libraries\")\n";
    str += "set(INSTALL_BIN_DIR bin CACHE PATH \"Installation directory for executables\")\n";
    str += "set(INSTALL_INCLUDE_DIR include CACHE PATH\n";
    str += "   \"Installation directory for header files\")\n";
    str += "if(WIN32 AND NOT CYGWIN)\n";
    str += "   set(DEF_INSTALL_CMAKE_DIR CMake)\n";
    str += "else()\n";
    str += std::string("   set(DEF_INSTALL_CMAKE_DIR lib/CMake/") + mProjectName + ")\n";
    str += "endif()\n";
    str += "set(INSTALL_CMAKE_DIR ${DEF_INSTALL_CMAKE_DIR} CACHE PATH\n";
    str += "   \"Installation directory for CMake files\")\n\n";

    str += "# Make relative paths absolute (needed later on)\n";
    str += "foreach(p LIB BIN INCLUDE CMAKE)\n";
    str += "   set(var INSTALL_${p}_DIR)\n";
    str += "   if(NOT IS_ABSOLUTE \"${${var}}\")\n";
    str += "      set(${var} \"${CMAKE_INSTALL_PREFIX}/${${var}}\")\n";
    str += "   endif()\n";
    str += "endforeach()\n\n";

    str += "# Add debug and release flags\n";
    str += "# Use from the command line with -DCMAKE_BUILD_TYPE=Release or Debug\n";
    str += "set(CMAKE_CXX_FLAGS_DEBUG \"${CMAKE_CXX_FLAGS_DEBUG} -Wall\")\n";
    str += "set(CMAKE_CXX_FLAGS_RELEASE \"${CMAKE_CXX_FLAGS_RELEASE} -Wall\")\n";

    str += "# External Packages\n";
    str += "if(NOT WIN32)\n";
    for(auto const &pkg : mBuildPkgs.getPackages())
        {
        addPackageDefines(pkg.getPkgName(), str);
        }
    str += "endif()\n\n";

    str += "# set up include directories\n";
    str += "include_directories(\n";

    OovStringVec const compNames = mCompTypes.getDefinedComponentNames();
    for(auto const &name : compNames)
        {
        eCompTypes compType = mCompTypes.getComponentType(name);
        if(compType == CT_StaticLib)
            str += std::string("   \"${PROJECT_SOURCE_DIR}/") + name + "\"\n";
        }
    str += "   )\n";
    str += "add_definitions(-std=c++11)\n\n";

    str += "# Add sub directories\n";
    for(auto const &name : compNames)
        {
        eCompTypes compType = mCompTypes.getComponentType(name);
        if(compType != CT_Unknown)
            {
            std::string compRelDir = name;
            str += std::string("add_subdirectory(") + compRelDir + ")\n";
            }
        }

    str += "# Add all targets to the build-tree export set\n";
    str += "export(TARGETS ";
    for(auto const &name : compNames)
        {
        eCompTypes compType = mCompTypes.getComponentType(name);
        if(compType != CT_Unknown)
            {
            str += ' ';
            str += makeIdentifierFromComponentName(name);
            }
        }
    str += "\n";
    str += "   FILE \"${PROJECT_BINARY_DIR}/" + mProjectName + "Targets.cmake\")\n\n";

    str += "# Export the package for use from the build-tree\n";
    str += "# (this registers the build-tree with a global CMake-registry)\n";
    str += std::string("export(PACKAGE ") + mProjectName + ")\n\n";

    str += std::string("# Create the ") +  mProjectName + "Config.cmake and " +
            mProjectName + "ConfigVersion files\n";
    str += "file(RELATIVE_PATH REL_INCLUDE_DIR \"${INSTALL_CMAKE_DIR}\"\n";
    str += "   \"${INSTALL_INCLUDE_DIR}\")\n";
    str += "# for the build tree\n";
    str += "set(CONF_INCLUDE_DIRS \"${PROJECT_SOURCE_DIR}\" \"${PROJECT_BINARY_DIR}\")\n";
    str += std::string("configure_file(") + mProjectName + "Config.cmake.in\n";
    str += std::string("   \"${PROJECT_BINARY_DIR}/") + mProjectName + "Config.cmake\" @ONLY)\n";
    str += "# for the install tree\n";
    str += "set(CONF_INCLUDE_DIRS \"\\${" + projNameUpper + "_CMAKE_DIR}/${REL_INCLUDE_DIR}\")\n";
    str += std::string("configure_file(") + mProjectName + "Config.cmake.in\n";
    str += std::string("   \"${PROJECT_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/") +
            mProjectName + "Config.cmake\" @ONLY)\n";
    str += "# for both\n";
    str += std::string("configure_file(") + mProjectName + "ConfigVersion.cmake.in\n";
    str += std::string("   \"${PROJECT_BINARY_DIR}/") + mProjectName +
            "ConfigVersion.cmake\" @ONLY)\n\n";

    str += std::string("# Install the ") + mProjectName + "Config.cmake and " +
            mProjectName + "ConfigVersion.cmake\n";
    str += "install(FILES\n";
    str += std::string("   \"${PROJECT_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/") +
            mProjectName + "Config.cmake\"\n";
    str += std::string("   \"${PROJECT_BINARY_DIR}/") + mProjectName +
            "ConfigVersion.cmake\"\n";
    str += "DESTINATION \"${INSTALL_CMAKE_DIR}\" COMPONENT dev)\n\n";

    str += "# Install the export set for use with the install-tree\n";
    str += std::string("install(EXPORT ") + mProjectName + "Targets DESTINATION\n";
    str += "   \"${INSTALL_CMAKE_DIR}\" COMPONENT dev)\n";
    OovStatus status = writeFile(destName, str);
    if(status.needReport())
        {
        OovString err = "Unable to make top makelist ";
        err += destName;
        status.report(ET_Error, err);
        }
    return status;
    }

OovStatusReturn CMaker::makeTopInFile(OovStringRef const destName)
    {
    OovString projNameUpper;
    projNameUpper.setUpperCase(mProjectName);
    OovStringVec const compNames = mCompTypes.getDefinedComponentNames();

    std::string str = std::string("# - Config file for the ") + mProjectName + " package\n";

    str += "# Compute paths\n";
    str += std::string("get_filename_component(") + projNameUpper +
            "_CMAKE_DIR \"${CMAKE_CURRENT_LIST_FILE}\" PATH)\n";
    str += std::string("set(") + projNameUpper + "_INCLUDE_DIRS \"@CONF_INCLUDE_DIRS@\")\n\n";

    str += "# Our library dependencies (contains definitions for IMPORTED targets)\n";
    str += std::string("if(NOT TARGET ") + mProjectName + "AND NOT " +
            projNameUpper + "_BINARY_DIR)\n";
    str += "   include(\"${FOOBAR_CMAKE_DIR}/" + mProjectName + "Targets.cmake\")\n";
    str += "endif()\n\n";

    str += std::string("# These are IMPORTED targets created by ") + mProjectName +
            "Targets.cmake\n";
    str += std::string("set(") + projNameUpper + "_LIBRARIES";
    for(auto const &name : compNames)
        {
        eCompTypes compType = mCompTypes.getComponentType(name);
        if(compType == CT_StaticLib)
            {
            str += ' ';
            str += name;
            }
        }
    str += ")\n";
    str += std::string("set(") + projNameUpper + "_EXECUTABLE";
    for(auto const &name : compNames)
        {
        eCompTypes compType = mCompTypes.getComponentType(name);
        if(compType == CT_Program)
            {
            str += ' ';
            str += name;
            }
        }
    str += ")\n";
    OovStatus status = writeFile(destName, str);
    if(status.needReport())
        {
        OovString err = "Unable to make top in file ";
        err += destName;
        status.report(ET_Error, err);
        }
    return status;
    }

OovStatusReturn CMaker::makeTopVerInFile(OovStringRef const destName)
    {
    OovString projNameUpper;
    projNameUpper.setUpperCase(mProjectName);

    std::string str;
    str += "set(PACKAGE_VERSION \"@" + projNameUpper + "_VERSION@\")\n\n";

    str += "# Check whether the requested PACKAGE_FIND_VERSION is compatible\n";
    str += "if(\"${PACKAGE_VERSION}\" VERSION_LESS \"${PACKAGE_FIND_VERSION}\")\n";
    str += "   set(PACKAGE_VERSION_COMPATIBLE FALSE)\n";
    str += "else()\n";
    str += "   set(PACKAGE_VERSION_COMPATIBLE TRUE)\n";
    str += "   if (\"${PACKAGE_VERSION}\" VERSION_EQUAL \"${PACKAGE_FIND_VERSION}\")\n";
    str += "      set(PACKAGE_VERSION_EXACT TRUE)\n";
    str += "   endif()\n";
    str += "endif()\n";
    OovStatus status = writeFile(destName, str);
    if(status.needReport())
        {
        OovString err = "Unable to make ver file ";
        err += destName;
        status.report(ET_Error, err);
        }
    return status;
    }

OovStatusReturn CMaker::makeToolchainFile(OovStringRef const compilePath,
        OovStringRef const destFileName)
    {
    std::string str;
    str += "SET(CMAKE_SYSTEM_NAME Linux)\n";
    str += "SET(CMAKE_SYSTEM_PROCESSOR \"arm\")\n";
    str += "include(CMakeForceCompiler)\n";
//    str += "CMAKE_FORCE_C_COMPILER(arm-linux-gnueabihf-gcc GNU)\n";
    str += "CMAKE_FORCE_CXX_COMPILER(";
    str += compilePath;
    str += " GNU)\n";

    OovStatus status = writeFile(destFileName, str);
    if(status.needReport())
        {
        OovString err = "Unable to make toolchain file ";
        err += destFileName;
        status.report(ET_Error, err);
        }
    return status;
    }

void maybeSplitLine(std::string &str)
    {
    size_t startpos = str.rfind('\n');
    if(startpos == std::string::npos)
        startpos = 0;
    if(str.length() - startpos > 70)
        {
        str += "\n  ";
        }
    }

void CMaker::appendNames(OovStringVec const &names,
        char delim, OovString &str)
    {
    for(auto const &name : names)
        {
        str += name;
        str += delim;
        maybeSplitLine(str);
        }
    int len = str.length();
    if(len > 0 && str[len-1] == delim)
        str.resize(len-1);
    }

enum eCommandTypes { CT_Exec, CT_Shared, CT_Static,
    CT_Interface, // A library with only headers
    CT_TargHeaders, CT_TargLinkLibs };

static void addCommandAndNames(eCommandTypes ct, char const *compName,
        OovStringVec const &names, OovString &str)
    {
    switch(ct)
        {
        case CT_Shared:
        case CT_Static:
        case CT_Interface:
            str += "add_library";
            break;

        case CT_Exec:           str += "add_executable";        break;
        case CT_TargHeaders:    str += "set";                   break;
        case CT_TargLinkLibs:   str += "target_link_libraries"; break;
        }
    str += "(";
    if(ct == CT_TargHeaders)
        {
        str += "HEADER_FILES ";
        }
    else
        {
        str += compName;
        }
    switch(ct)
        {
        case CT_Exec:           str += ' ';             break;
        case CT_Shared:         str += " SHARED ";      break;
        case CT_Static:         str += " STATIC ";      break;
        // Using INTERFACE at this time gives:
        // "Cannot find source file:", since CMake doesn't understand interface keyword.
//      case CT_Interface:      str += " INTERFACE ";   break;
        case CT_Interface:      str += " STATIC ";      break;
        case CT_TargHeaders:    str += ' ';             break;
        case CT_TargLinkLibs:   str += ' ';             break;
        }
    CMaker::appendNames(names, ' ', str);
    str += ")\n\n";
    if(ct == CT_TargHeaders)
        {
        str += "set_target_properties(";
        str += compName;
        str += " PROPERTIES PUBLIC_HEADER \"${HEADER_FILES}\")\n\n";
        }
    }

void CMaker::addLibsAndIncs(OovStringRef const compName, OovString &str)
    {
    OovStringVec extraIncs;
    OovStringVec libs = getCompLibrariesAndIncs(compName, extraIncs);
    addCommandAndNames(CT_TargLinkLibs, compName, libs, str);

    if(extraIncs.size())
        {
        str += "include_directories(";
        appendNames(extraIncs, ',', str);
        str += ")\n\n";
        }
    }


OovString CMaker::makeJavaComponentFile(OovStringRef const compName,
    eCompTypes compType,
    OovStringVec const &source, OovStringRef const destName)
    {
    OovString str;
    str += "cmake_minimum_required (VERSION 2.8)\n\n"
        "find_package(Java REQUIRED)\n"
        "include(UseJava)\n"
        "enable_testing()\n"
        "project (";
    str += compName;
    str += ")\n";
    str += "set(CMAKE_JAVA_COMPILE_FLAGS \"-source\" \"1.6\" \"-target\" \"1.6\")\n\n";

    str += "add_jar(";
    str += compName;
    str += "\n";
    if(compType == CT_JavaJarProg)
        {
        str += "   MANIFEST ${CMAKE_CURRENT_SOURCE_DIR}/Manifest.txt\n";
        }
    str += "   ";
    // @todo - Add java files
    OovStringVec srcFiles = mScannedComponentInfo.getComponentFiles(mCompTypes,
        ScannedComponentInfo::CFT_JavaSource, compName);
    for(auto const &src : srcFiles)
        {
        FilePath compSourcePath(Project::getSrcRootDirectory(), FP_Dir);
        compSourcePath.appendDir(compName);
        OovString srcFn = Project::getSrcRootDirRelativeSrcFileName(src,
            compSourcePath);
        str += srcFn;
        maybeSplitLine(str);
        str += ' ';
        }
    // @todo - Add jar files
    str += ")\n\n";

    str += "add_test(NAME ";
    str += "Test";
    str += compName;
    str += " COMMAND ${Java_JAVA_EXECUTABLE} -cp ${_jarFile} ";
    str += compName;
    str += ")\n";
    str += "install_jar(oovJavaParser ${INSTALL_BIN_DIR})\n";
    return str;
    }

OovStatusReturn CMaker::makeComponentFile(OovStringRef const compName,
    eCompTypes compType,
    OovStringVec const &sources, OovStringRef const destName)
    {
    OovString str;
    if(mVerbose)
        printf("Processing %s\n      %s\n", compName.getStr(), destName.getStr());
    switch(compType)
        {
        case CT_Program:
            {
            if(mVerbose)
                printf("  Executable\n");

            addCommandAndNames(CT_Exec, compName, sources, str);

            addLibsAndIncs(compName, str);

            str += "install(TARGETS ";
            str += compName;
            str += "\n  EXPORT ";
            str += mProjectName;
            str += "Targets";
            str += "\n  RUNTIME DESTINATION \"${INSTALL_BIN_DIR}\" COMPONENT lib)\n";
            }
            break;

        case  CT_SharedLib:
            {
            if(mVerbose)
                printf("  SharedLib\n");
            addCommandAndNames(CT_Shared, compName, sources, str);

            addLibsAndIncs(compName, str);

            str += "install(TARGETS ";
            str += compName;
            str += "\n  LIBRARY DESTINATION \"${INSTALL_LIB_DIR}\" COMPONENT lib)\n";
            }
            break;

        case CT_StaticLib:
            {
            if(mVerbose)
                printf("  Library\n");
            OovStringVec headers = mScannedComponentInfo.getComponentFiles(
                mCompTypes, ScannedComponentInfo::CFT_CppInclude, compName);
            discardDirs(headers);

            OovStringVec allFiles = headers;
            allFiles.insert(allFiles.end(), sources.begin(), sources.end() );
            std::sort(allFiles.begin(), allFiles.end(), compareNoCase);
            if(sources.size() == 0)
                {
                addCommandAndNames(CT_Interface, compName, allFiles, str);
                str += "set_target_properties(";
                str += compName;
                str += " PROPERTIES LINKER_LANGUAGE CXX)\n";
                }
            else
                addCommandAndNames(CT_Static, compName, allFiles, str);

            addCommandAndNames(CT_TargHeaders, compName, headers, str);


            str += "install(TARGETS ";
            str += compName;
            str += "\n  EXPORT ";
            str += mProjectName;
            str += "Targets";
            str += "\n  ARCHIVE DESTINATION \"${INSTALL_LIB_DIR}\" COMPONENT lib";
            str += "\n  PUBLIC_HEADER DESTINATION \"${INSTALL_INCLUDE_DIR}/";
            str += mProjectName;
            str += "\" COMPONENT dev)\n";
            }
            break;

        case CT_JavaJarLib:
            {
            if(mVerbose)
                printf("  JavaLib\n");
            str += makeJavaComponentFile(compName, compType, sources, destName);
            }
            break;

        case CT_JavaJarProg:
            if(mVerbose)
                printf("  JavaProg\n");
            str += makeJavaComponentFile(compName, compType, sources, destName);
            break;

        case CT_Unknown:
            break;
        }

    OovStatus status = writeFile(destName, str);
    if(status.needReport())
        {
        OovString err = "Unable to make component file ";
        err += destName;
        status.report(ET_Error, err);
        }
    return status;
    }

OovStringVec CMaker::getCompSources(OovStringRef const compName)
    {
    OovStringVec sources = mScannedComponentInfo.getComponentFiles(mCompTypes,
        ScannedComponentInfo::CFT_CppSource, compName);
    OovString compPath = mCompTypes.getComponentAbsolutePath(compName);
    for(auto &src : sources)
        {
        src.erase(0, compPath.length());
        }
    std::sort(sources.begin(), sources.end(), compareNoCase);
    return sources;
    }

OovStringVec CMaker::getCompLibrariesAndIncs(OovStringRef const compName,
        OovStringVec &extraIncDirs)
    {
    OovStringVec libs;
/*
    OovStringVec projectLibFileNames;
    mObjSymbols.appendOrderedLibFileNames("ProjLibs", getSymbolBasePath(),
            projectLibFileNames);
*/
    /// @todo - this does not order the libraries.
    OovStringVec srcFiles = mScannedComponentInfo.getComponentFiles(
        mCompTypes, ScannedComponentInfo::CFT_CppSource, compName);
    OovStringSet projLibs;
    OovStringSet extraIncDirsSet;
    OovStringVec compNames = mCompTypes.getDefinedComponentNames();
    for(auto const &srcFile : srcFiles)
        {
        FilePath fp;
        fp.getAbsolutePath(srcFile, FP_File);
        OovStringVec incDirs =
            mIncMap.getNestedIncludeDirsUsedBySourceFile(fp);
        for(auto const &supplierCompName : compNames)
            {
            eCompTypes compType = mCompTypes.getComponentType(supplierCompName);
            if(compType == CT_StaticLib || compType == CT_Unknown)
                {
                std::string compDir = mCompTypes.getComponentAbsolutePath(
                    supplierCompName);
                for(auto const &incDir : incDirs)
                    {
                    if(compDir.compare(incDir) == 0)
                        {
                        if(supplierCompName.compare(compName) != 0)
                            {
                            if(compType == CT_StaticLib)
                                {
                                projLibs.insert(makeIdentifierFromComponentName(
                                    supplierCompName));
                                }
                            else
                                {
                                /// @todo - this could check for include files in the dir
                                extraIncDirsSet.insert(makeRelativeIdentifierFromComponentName(
                                    compName, supplierCompName));
                                }
                            break;
                            }
                        }
                    }
                }
            }
        }
    std::copy(projLibs.begin(), projLibs.end(), std::back_inserter(libs));
    std::copy(extraIncDirsSet.begin(), extraIncDirsSet.end(),
            std::back_inserter(extraIncDirs));

    for(auto const &pkg : mBuildPkgs.getPackages())
        {
        OovString compDir = mCompTypes.getComponentAbsolutePath(compName);
        OovStringVec incRoots = pkg.getIncludeDirs();
        if(mIncMap.anyRootDirsMatch(incRoots, compDir))
            {
            if(pkg.getPkgName().compare(compName) != 0)
                {
                OovString pkgRef = "${";
                OovString pkgDefName;
                makeDefineName(pkg.getPkgName(), pkgDefName);
                pkgRef += pkgDefName + "_LIBRARIES}";
                libs.push_back(pkgRef);
                }
            }
        }
    return libs;
    }

OovStatusReturn CMaker::makeTopLevelFiles(OovStringRef const outDir)
    {
    FilePath topVerInFp(outDir, FP_File);
    topVerInFp.appendFile(std::string(mProjectName + "ConfigVersion.cmake.in"));
    OovStatus status = makeTopVerInFile(topVerInFp);

    if(status.ok())
        {
        FilePath topInFp(outDir, FP_File);
        topInFp.appendFile(std::string(mProjectName + "Config.cmake.in"));
        status = makeTopInFile(topInFp);
        }
    return status;
    }

OovStatusReturn CMaker::makeToolchainFiles(OovStringRef const outDir)
    {
    OovStatus status(true, SC_File);
    status = mProject.readProject(Project::getProjectDirectory());
    if(status.ok())
        {
        ProjectBuildArgs buildArgs(mProject);

        std::string buildConfigStr = mProject.getValue(OptBuildConfigs);
        if(buildConfigStr.length() > 0)
            {
            CompoundValue buildConfigs(buildConfigStr);
            for(auto const &config : buildConfigs)
                {
                buildArgs.setConfig(OptFilterValueBuildModeBuild, config);
/// @todo - The compiler settings must be more sophisticated if each
/// component has different settings.
//                buildArgs.setCompConfig();
                OovString compilePath = buildArgs.getCompilerPath();

                FilePath outFp(outDir, FP_Dir);
                std::string fn = config + ".cmake";
                outFp.appendFile(fn);
                status = makeToolchainFile(compilePath, outFp);
                }
            }
        }
    return status;
    }

// outDir ignored if writeToProject is true.
OovStatusReturn CMaker::makeComponentFiles(bool writeToProject,
    OovStringRef const outDir, OovStringVec const &compNames)
    {
    OovStatus status(true, SC_File);
    FilePath incMapFn(getAnalysisPath(), FP_Dir);
    incMapFn.appendFile(Project::getAnalysisIncDepsFilename());
    status = mIncMap.read(incMapFn);
    if(status.ok())
        {
        if(mVerbose)
            printf("Read incmap\n");
        }
    else
        {
        status.reported();
        }
    for(auto const &compName : compNames)
        {
        eCompTypes compType = mCompTypes.getComponentType(compName);
        if(compType != CT_Unknown)
            {
            OovStringVec sources = getCompSources(compName);
            FilePath outFp;
            std::string fixedCompName = makeIdentifierFromComponentName(compName);
            if(writeToProject)
                {
                outFp.setPath(mCompTypes.getComponentAbsolutePath( compName),
                    FP_Dir);
                outFp.appendFile("CMakeLists.txt");
                }
            else
                {
                outFp.setPath(outDir, FP_File);
                outFp.appendFile(std::string(fixedCompName + "-CMakeLists.txt"));
                }
            // Using the filepath here gives:
            // "Error evaluating generator expression", and "Target name not supported"
            status = makeComponentFile(fixedCompName, compType, sources, outFp);
            }
        if(!status.ok())
            {
            break;
            }
        }
    return status;
    }

/*
OovString CMaker::getComponentAbsolutePath(OovStringRef const compName)
    {
    auto const mapIter = mCachedComponentPaths.find(compName);
    if(mapIter == mCachedComponentPaths.end())
        {
        mCachedComponentPaths[compName] = mCompTypes.getComponentAbsolutePath(compName);
        }
    return mCachedComponentPaths[compName];
    }
*/

bool CMaker::makeFiles(bool writeToProject)
    {
    FilePath outDir;
    bool success = false;
    OovStatus status(true, SC_File);
    if(writeToProject)
        {
        outDir.setPath(Project::getSrcRootDirectory(), FP_Dir);
        }
    else
        {
        outDir.setPath(Project::getBuildOutputDir("CMake"), FP_Dir);
        status = FileEnsurePathExists(outDir);
        if(mVerbose)
            printf("Output directory %s\n", outDir.getStr());
        }
    if(status.needReport())
        {
        status.report(ET_Error, "Unable to make output directory");
        }
    if(status.ok())
        {
        status = mScannedComponentInfo.readScannedInfo();
        }
    if(status.ok())
        {
        status = makeToolchainFiles(outDir);
        }
    if(status.ok())
        {
        status = makeTopLevelFiles(outDir);
        }
    if(status.ok())
        {
        status = mBuildPkgs.read();
        if(status.needReport())
            {
            // The build package file is optional.
            status.clearError();
            }
        }
    if(status.ok())
        {
        FilePath topMlFp(outDir, FP_File);
        topMlFp.appendFile("CMakeLists.txt");
        status = makeTopMakelistsFile(topMlFp);
        }
    if(status.ok())
        {
        OovStringVec compNames = mCompTypes.getDefinedComponentNames();
        if(compNames.size() > 0)
            {
            status = makeComponentFiles(writeToProject, outDir, compNames);
            if(status.ok())
                {
                success = true;
                }
            }
        else
            fprintf(stderr, "Components must be defined\n");
        }
    else
        fprintf(stderr, "Unable to read project files\n");
    return success;
    }


int main(int argc, char * argv[])
    {
    int ret = 1;
    bool verbose = false;
    bool writeToProject = false;
    const char *projDir = nullptr;
    const char *projName = "PROJNAME";
    OovError::setComponent(EC_OovCMaker);

    for(int argi=1; argi<argc; argi++)
        {
        if(argv[argi][0] == '-')
            {
            switch(argv[argi][1])
                {
                case 'v':
                    verbose = true;
                    break;

                case 'w':
                    writeToProject = true;
                    break;

                case 'n':
                    projName = &argv[argi][2];
                    break;
                }
            }
        else
            projDir = argv[argi];
        }
    if(projDir)
        {
        Project::setProjectDirectory(projDir);
        CMaker maker(projName, verbose);
        if(maker.makeFiles(writeToProject))
            {
            ret = 0;
            }
        }
    else
        {
        fprintf(stderr, "OovCMaker version %s\n", OOV_VERSION);
        fprintf(stderr, "  OovCMaker projectDirectory [switches]\n");
        fprintf(stderr, "    switches -v=verbose, -w=write to project, -nProjName\n");
        }
    return ret;
    }
