/*
 * ObjSymbols.cpp
 *
 *  Created on: Sep 13, 2013
 *  \copyright 2013 DCBlaha.  Distributed under the GPL.
 */

#include "ObjSymbols.h"
#include <string>
#include <stdio.h>
#include <algorithm>
#include "OovProcess.h"
#include "ComponentBuilder.h"
#include "OovError.h"

class FileSymbol
    {
    public:
        FileSymbol(OovStringRef const symbolName, unsigned int len,
            size_t fileIndex):
            mSymbolName(symbolName, len), mFileIndex(fileIndex)
            {}
        static const size_t NoFileIndex = static_cast<size_t>(-1);
        // For sorting and searching
        bool operator<(const FileSymbol &rh) const
            { return(mSymbolName < rh.mSymbolName); }
        OovString mSymbolName;
        size_t mFileIndex;
    };

// The first is the client file index, and the second is a container of supplier file indices.
class FileDependencies:public std::map<size_t, std::set<size_t> >
    {
    public:
        void addDependency(size_t clientIndex, size_t supplierIndex);
        // Check if the file index passed in is dependent on any other files.
        bool dependentOnAny(size_t fileIndex) const;
        // Remove all references to the file index.
        void removeValue(size_t fileIndex);

    private:
        // Remove a client if there are no suppliers
        bool removeEmptyClient();
    };

void FileDependencies::addDependency(size_t clientIndex, size_t supplierIndex)
    {
    auto iter = find(clientIndex);
    if(iter == end())
        {
        std::set<size_t> suppliers;
        suppliers.insert(supplierIndex);
        insert(std::pair<size_t, std::set<size_t> >(clientIndex, suppliers));
        }
    else
        {
        iter->second.insert(supplierIndex);
        }
    }

bool FileDependencies::dependentOnAny(size_t clientIndex) const
    {
    return(find(clientIndex) != end());
    }

void FileDependencies::removeValue(size_t fileIndex)
    {
    for(auto &dep : *this)
        {
        dep.second.erase(fileIndex);
        }
    while(removeEmptyClient())
        {
        }
    }

bool FileDependencies::removeEmptyClient()
    {
    bool removed = false;
    for(auto dep=begin(); dep != end(); dep++)
        {
        if((*dep).second.size() == 0)
            {
            erase(dep);
            removed = true;
            break;
            }
        }
    return removed;
    }


class FileSymbols:public std::set<FileSymbol>
    {
    public:
        void add(OovStringRef const symbolName, unsigned int len,
            size_t fileIndex)
            { insert(FileSymbol(symbolName, len, fileIndex)); }
        std::set<FileSymbol>::iterator findName(OovStringRef const symbolName)
            {
            FileSymbol fs(symbolName, std::string(symbolName).length(), FileSymbol::NoFileIndex);
            return find(fs);
            }
        bool writeSymbols(OovStringRef const symFileName);
    };

bool FileSymbols::writeSymbols(OovStringRef const symFileName)
    {
    bool success = false;
    File file;
    OovStatus status = file.open(symFileName, "w");
    if(status.ok())
        {
        for(const auto &symbol : *this)
            {
            OovString str = symbol.mSymbolName;
            str += ' ';
            str.appendInt(static_cast<int>(symbol.mFileIndex));
            str += '\n';
            status = file.putString(str);
            if(!status.ok())
                {
                break;
                }
            }
        }
    if(status.needReport())
        {
        OovString errStr = "Unable to write symbol file: ";
        errStr += symFileName;
        status.report(ET_Error, errStr);
        }
    return(success);
    }


class FileList:public OovStringVec
    {
    public:
        OovStatusReturn writeFile(File const &file) const;
    };

OovStatusReturn FileList::writeFile(File const &file) const
    {
    OovStatus status(true, SC_File);
    for(const auto &fn : (*this))
        {
        OovString str = "f:";
        str += fn;
        str += '\n';
        status = file.putString(str);
        if(!status.ok())
            {
            break;
            }
        }
    if(status.needReport())
        {
        status.report(ET_Error, "Unable to write symbol file");
        }
    return status;
    }


class FileIndices:public std::vector<size_t>
    {
    public:
        void removeValue(size_t val);
        iterator findValue(size_t val);
    };


class ClumpSymbols
    {
    public:
        // Parse files for:
        // U referenced objects, but not defined
        // D global data object
        // T global function object
        bool addSymbols(OovStringRef const libFilePath, OovStringRef const outFileName);
        void writeClumpFiles(OovStringRef const clumpName,
                OovStringRef const outPath);
    private:
        FileSymbols mDefinedSymbols;
        FileSymbols mUndefinedSymbols;
        FileDependencies mFileDependencies;
        FileList mFileIndices;
        FileIndices mOrderedDependencies;
        std::mutex mDataMutex;
        void resolveUndefinedSymbols();
        bool readRawSymbolFile(OovStringRef const outRawFileName, size_t fileIndex);
    };


void FileIndices::removeValue(size_t val)
    {
    const auto &pos = findValue(val);
    if(pos != end())
        erase(pos);
    }

std::vector<size_t>::iterator FileIndices::findValue(size_t val)
    {
    iterator retiter = end();
    for(iterator iter=begin(); iter!=end(); ++iter)
        {
        if(val == (*iter))
            {
            retiter = iter;
            break;
            }
        }
    return(retiter);
    }


// The .dep file is the master file. The filenames are listed first starting
// with "f:". The are listed in order, so the first file is file index 0.
// Then the "o:" indicates the order that the files should be linked.
static bool writeDepFileInfo(OovStringRef const symFileName, const FileList &fileIndices,
        const FileDependencies &fileDeps, const FileIndices &orderedDeps)
    {
    File file;
    OovStatus status = file.open(symFileName, "w");
    if(status.ok())
        {
        status = fileIndices.writeFile(file);
        if(status.ok())
            {
            for(const auto &dep : fileDeps)
                {
                OovString str = "d:";
                str.appendInt(static_cast<int>(dep.first));
                str += ' ';
                for(const auto &sup : dep.second)
                    {
                    str.appendInt(static_cast<int>(sup));
                    str += ' ';
                    }
                str += '\n';
                status = file.putString(str);
                if(!status.ok())
                    {
                    break;
                    }
                }
            }
        if(status.ok())
            {
            OovString str = "o:";
            for(const auto &dep : orderedDeps)
                {
                str.appendInt(static_cast<int>(dep));
                str += ' ';
                }
            str += '\n';
            status = file.putString(str);
            }
        }
    if(status.needReport())
        {
        OovString str = "Unable to write dependency symbol file: ";
        str += symFileName;
        status.report(ET_Error, str);
        }
    return(status.ok());
    }

static void orderDependencies(size_t numFiles, const FileDependencies &fileDependencies,
        FileIndices &orderedDependencies)
    {
    FileDependencies fileDeps = fileDependencies;
    FileIndices origFiles;
    for(size_t i=0; i<numFiles; i++)
        {
        origFiles.push_back(i);
        }
    // Go through all libraries
    for(size_t totaliter=0; totaliter<numFiles; totaliter++)
        {
        if(origFiles.size() == 0)
            break;
        // If any library is not dependent on any others, then remove it
        // as a client or provider for all libraries.
        for(size_t i=0; i<origFiles.size(); i++)
            {
            size_t fileIndex = origFiles[i];
            if(!fileDeps.dependentOnAny(fileIndex))
                {
                orderedDependencies.insert(orderedDependencies.begin()+0, fileIndex);
                origFiles.removeValue(fileIndex);
                fileDeps.removeValue(fileIndex);
                break;
                }
            }
        }
    // If there were any left over, there were circular dependencies.
    for(size_t i=0; i<origFiles.size(); i++)
        {
        size_t fileIndex = origFiles[i];
        orderedDependencies.insert(orderedDependencies.begin()+0, fileIndex);
        }
    }

bool ClumpSymbols::readRawSymbolFile(OovStringRef const outRawFileName,
    size_t fileIndex)
    {
    File inFile;
    OovStatus status = inFile.open(outRawFileName, "r");
    if(status.ok())
        {
        char buf[2000];
        while(inFile.getString(buf, sizeof(buf), status))
            {
            char const *p = buf;
            while(isspace(*p) || isxdigit(*p))
                p++;
            char symTypeChar = *p;

            p++;
            while(isspace(*p))
                p++;
            char const *endP = p;
            while(!isspace(*endP))
                endP++;
            if(symTypeChar == 'D' || symTypeChar == 'T')
                {
                std::unique_lock<std::mutex> lock(mDataMutex);
                mDefinedSymbols.add(p, static_cast<size_t>(endP-p), fileIndex);
                }
            else if(symTypeChar == 'U')
                {
                std::unique_lock<std::mutex> lock(mDataMutex);
                mUndefinedSymbols.add(p, static_cast<size_t>(endP-p), fileIndex);
                }
            }
        }
    if(status.needReport())
        {
        OovString str = "Unable to read raw symbol file: ";
        str += outRawFileName;
        status.report(ET_Error, str);
        }
    return status.ok();
    }

void ClumpSymbols::resolveUndefinedSymbols()
    {
    for(const auto &sym : mDefinedSymbols)
        {
        auto pos = mUndefinedSymbols.findName(sym.mSymbolName);
        if(pos != mUndefinedSymbols.end())
            {
            // The file indices can be the same if one library has
            // an object file where it is defined, and another object
            // file where it is not defined.
            if((*pos).mFileIndex != sym.mFileIndex)
                {
                mFileDependencies.addDependency((*pos).mFileIndex, sym.mFileIndex);
                }
            mUndefinedSymbols.erase(pos);
            }
        }
    }

bool ClumpSymbols::addSymbols(OovStringRef const libFilePath, OovStringRef const outRawFileName)
    {
        {
        std::unique_lock<std::mutex> lock(mDataMutex);
        mFileIndices.push_back(libFilePath);
        }
    return readRawSymbolFile(outRawFileName, mFileIndices.size()-1);
    }

void ClumpSymbols::writeClumpFiles(OovStringRef const clumpName,
        OovStringRef const outPath)
    {
    resolveUndefinedSymbols();
    orderDependencies(mFileIndices.size(), mFileDependencies, mOrderedDependencies);

    OovString defSymFileName = outPath;
    OovString undefSymFileName = defSymFileName;
    OovString depSymFileName = defSymFileName;
    undefSymFileName += "LibSym-";
    undefSymFileName += clumpName;
    undefSymFileName += "-Undef.txt";

    defSymFileName += "LibSym-";
    defSymFileName += clumpName;
    defSymFileName += "-Def.txt";

    depSymFileName += "LibSym-";
    depSymFileName += clumpName;
    depSymFileName += "-Dep.txt";
    mDefinedSymbols.writeSymbols(defSymFileName);
    mUndefinedSymbols.writeSymbols(undefSymFileName);
    writeDepFileInfo(depSymFileName, mFileIndices, mFileDependencies,
            mOrderedDependencies);
    }

class ObjTaskListener:public TaskQueueListener
    {
    public:
        ObjTaskListener(ClumpSymbols &clumpSymbols):
            mClumpSymbols(clumpSymbols)
            {}
    private:
        ClumpSymbols &mClumpSymbols;
        virtual void extraProcessing(bool success, OovStringRef const outFile,
                OovStringRef const stdOutFn, ProcessArgs const &item) override;
    };

void ObjTaskListener::extraProcessing(bool success, OovStringRef const /*outFile*/,
    OovStringRef const stdOutFn, ProcessArgs const &item)
    {
    if(success)
        {
        mClumpSymbols.addSymbols(item.mLibFilePath, stdOutFn);
        }
    }

bool ObjSymbols::makeObjectSymbols(OovStringVec const &libFiles,
        OovStringRef const outSymPath, OovStringRef const objSymbolTool,
        ComponentTaskQueue &queue, ClumpSymbols &clumpSymbols)
    {
    struct LibFileNames
        {
        LibFileNames(OovString const &libFileName,
                OovString const &libSymFileName,
                OovString const &libFilePath, bool genSymbolFile):
            mLibFileName(libFileName), mLibFilePath(libFilePath),
            mLibSymFileName(libSymFileName), mGenSymbolFile(genSymbolFile)
            {}
        OovString mLibFileName;
        OovString mLibFilePath;
        OovString mLibSymFileName;
        bool mGenSymbolFile;
        };
    bool generatedSymbols = false;
    std::vector<LibFileNames> libFileNames;
    OovStatus status(true, SC_File);
    for(const auto &libFilePath : libFiles)
        {
        FilePath libSymName(libFilePath, FP_File);
        libSymName.discardDirectory();
        libSymName.discardExtension();
        OovString libSymPath = outSymPath;
        FilePathEnsureLastPathSep(libSymPath);
        status = FileEnsurePathExists(libSymPath);
        if(status.ok())
            {
            OovString outSymRawFileName = libSymPath + libSymName + ".txt";
/*
            std::string libName = libFilePath;
            size_t libNamePos = rfindPathSep(libName);
            if(libNamePos != std::string::npos)
                libNamePos++;
            else
                libNamePos = 0;
            libName.insert(libNamePos, "lib");
            std::string libFileName = // std::string(outLibPath) +
                libName + ".a";
*/
            if(FileStat::isOutputOld(outSymRawFileName, libFilePath, status))
                {
                libFileNames.push_back(LibFileNames(libFilePath, outSymRawFileName,
                        libFilePath, true));
                generatedSymbols = true;
                }
            else
                {
                libFileNames.push_back(LibFileNames(libFilePath, outSymRawFileName,
                        libFilePath, false));
                }
            }
        if(!status.ok())
            {
            break;
            }
        }

    if(generatedSymbols)
        {
#define MULTI_THREAD 1
#if(MULTI_THREAD)
        ObjTaskListener listener(clumpSymbols);
        queue.setTaskListener(&listener);
        queue.setupQueue(queue.getNumHardwareThreads());
#endif
        for(auto const &libFn : libFileNames)
            {
            if(libFn.mGenSymbolFile)
                {
                OovProcessChildArgs ca;
                ca.addArg(objSymbolTool);
                std::string quotedLibFilePath = libFn.mLibFilePath;
                FilePathQuoteCommandLinePath(quotedLibFilePath);
                ca.addArg(quotedLibFilePath);

                ProcessArgs procArgs(objSymbolTool, libFn.mLibSymFileName, ca,
                        libFn.mLibSymFileName.getStr());
                procArgs.mLibFilePath = libFn.mLibFilePath;
#if(MULTI_THREAD)
                queue.addTask(procArgs);
#else
                success = ComponentBuilder::runProcess(objSymbolTool,
                    outRawFileName, ca, mListenerStdMutex, outRawFileName);
                if(success)
                    clumpSymbols.addSymbols(libFilePath, outRawFileName);
#endif
                }
            else
                {
                clumpSymbols.addSymbols(libFn.mLibFilePath,
                        libFn.mLibSymFileName);
                }
            }
#if(MULTI_THREAD)
        queue.waitForCompletion();
        queue.setTaskListener(nullptr);
#endif
        }
    return generatedSymbols;
    }

bool ObjSymbols::makeClumpSymbols(OovStringRef const clumpName,
        OovStringVec const &libFiles, OovStringRef const outSymPath,
        OovStringRef const objSymbolTool, ComponentTaskQueue &queue)
    {
    bool success = true;
    ClumpSymbols clumpSymbols;
//    std::string defSymFileName = outSymPath;
//    ensureLastPathSep(defSymFileName);

//    defSymFileName += "LibSym-";
//    defSymFileName += clumpName;
//    defSymFileName += "-Def.txt";
    if(//!fileExists(defSymFileName) ||
            makeObjectSymbols(libFiles, outSymPath, objSymbolTool, queue, clumpSymbols))
        {
        clumpSymbols.writeClumpFiles(clumpName, outSymPath);
        }
    return success;
    }

bool ObjSymbols::appendOrderedLibFileNames(OovStringRef const clumpName,
        OovStringRef const outPath,
        OovStringVec &sortedLibFileNames)
    {
    FilePath depSymFileName(outPath, FP_Dir);
    depSymFileName += "LibSym-";
    depSymFileName += clumpName;
    depSymFileName += "-Dep.txt";

    File file;
    OovStatus status = file.open(depSymFileName.getStr(), "r");
    if(status.ok())
        {
        std::vector<std::string> filenames;
        std::vector<size_t> indices;
        char buf[500];
        while(file.getString(buf, sizeof(buf), status))
            {
            if(buf[0] == 'f')
                {
                std::string fn = &buf[2];
                size_t pos = fn.rfind('\n');
                if(pos)
                    fn.resize(pos);
                filenames.push_back(fn);
                }
            else if(buf[0] == 'o')
                {
                char const *p = &buf[2];
                while(*p)
                    {
                    if(isdigit(*p))
                        {
                        int iNum;
                        if(sscanf(p, "%u", &iNum) == 1)
                            {
                            size_t num = static_cast<size_t>(iNum);
                            indices.push_back(num);
                            while(isdigit(*p))
                                p++;
                            }
                        }
                    else
                        p++;
                    }
                }
            }
        for(const auto &fileIndex : indices)
            {
            sortedLibFileNames.push_back(filenames[fileIndex]);
            }
        }
    if(status.needReport())
        {
        // Dependency symbol files are optional if there were no libraries in the project.
        status.clearError();
//        OovString errStr = "Unable to read dependency symbol file: ";
//        errStr += depSymFileName;
//        status.report(ET_Error, errStr);
        }
    return status.ok();
    }

void ObjSymbols::appendOrderedLibs(OovStringRef const clumpName,
        OovStringRef const outPath, OovStringVec &libDirs,
        OovStringVec &orderedLibNames)
    {
    OovStringVec orderedLibFileNames;
    appendOrderedLibFileNames(clumpName, outPath, orderedLibFileNames);
    OovStringSet dirs;

    for(auto const &lib : orderedLibFileNames)
        {
        FilePath fp(lib, FP_File);
        orderedLibNames.push_back(fp.getNameExt());
        dirs.insert(fp.getDrivePath());
        }
    std::copy(dirs.begin(), dirs.end(), std::back_inserter(libDirs));
    }
