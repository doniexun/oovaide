/*
 * OovProject.cpp
 *
 *  Created on: Feb 26, 2015
 *  \copyright 2015 DCBlaha.  Distributed under the GPL.
 */

#include "OovProject.h"
#include "Project.h"
#include "ProjectSettingsDialog.h"
#include "FilePath.h"
#include "Options.h"
#include "BuildConfigReader.h"
#include "DirList.h"
#include "Xmi2Object.h"
#include "Debug.h"
#include "OovError.h"

#define DEBUG_PROJ 0
#if(DEBUG_PROJ)
static DebugFile sProjFile("DebugProj.txt", "w");
static void logProj(char const *str)
    {
    sProjFile.printflush("%s\n", str);
    }
#else
#define logProj(str);
#endif


OovProject::~OovProject()
    {
    }

bool OovProject::newProject(OovString projectDir, CompoundValue const &excludeDirs,
        OovProject::eNewProjectStatus &projStat)
    {
    bool started = isProjectIdle();
    projStat = NP_CantCreateDir;
    if(started)
        {
        if(projectDir.length())
            {
            FilePathEnsureLastPathSep(projectDir);
            OovStatus status = FileEnsurePathExists(projectDir);
            if(status.ok())
                {
                Project::setProjectDirectory(projectDir);
                mProjectOptions.setFilename(Project::getProjectFilePath());
                mGuiOptions.setFilename(Project::getGuiOptionsFilePath());

                OptionsDefaults optionDefaults(mProjectOptions);
                optionDefaults.setDefaultOptions();
                mGuiOptions.setDefaultOptions();

                mProjectOptions.setNameValue(OptProjectExcludeDirs, excludeDirs.getAsString(';'));
                status = mProjectOptions.writeFile();
                if(status.ok())
                    {
                    status = mGuiOptions.writeFile();
                    if(!status.ok())
                        {
                        OovString errStr = "Unable to write GUI options file: ";
                        errStr += mGuiOptions.getFilename();
                        status.report(ET_Error, errStr);
                        }
                    bool openedProject = false;
                    // Return is discarded because isProjectIdle was checked previously.
                    openProject(projectDir, openedProject);
                    if(openedProject)
                        {
                        projStat = NP_CreatedProject;
                        }
                    }
                else
                    {
                    projStat = NP_CantCreateFile;
                    }
                }
            else
                {
                projStat = NP_CantCreateDir;
                }
            }
        }
    return started;
    }

bool OovProject::openProject(OovStringRef projectDir, bool &openedProject)
    {
    bool started = isProjectIdle();
    openedProject = false;
    OovStatus status(true, SC_File);
    if(started)
        {
        mProjectStatus.clear();
        Project::setProjectDirectory(projectDir);
        status = mProjectOptions.readProject(projectDir);
        if(status.ok())
            {
            if(FileIsDirOnDisk(Project::getSourceRootDirectory(), status))
                {
                openedProject = true;
                }
            else
                {
                Gui::messageBox("Unable to find source directory");
                ProjectSettingsDialog dlg(Gui::getMainWindow(),
                    getProjectOptions(), getGuiOptions(),
                    ProjectSettingsDialog::PS_OpenProjectEditSource);
                if(dlg.runDialog())
                    {
                    status = mProjectOptions.writeFile();
                    openedProject = true;
                    }
                }
            if(openedProject)
                {
                status = mGuiOptions.read();
                if(status.needReport())
                    {
                    mGuiOptions.setDefaultOptions();
                    status.report(ET_Error, "Unable to read GUI options for project, using defaults");
                    }
                }
            }
        mProjectStatus.mProjectOpen = openedProject;
        }
    if(status.needReport())
        {
        status.report(ET_Error, "Unable to open project");
        }
    return started;
    }

bool OovProject::clearAnalysis()
    {
    bool started = isProjectIdle();
    if(started)
        {
        logProj(" clearAnalysis");
        mProjectStatus.mAnalysisStatus = ProjectStatus::AS_UnLoaded;
        mModelData.clear();
        }
    return started;
    }

bool OovProject::loadAnalysisFiles()
    {
    bool started = isProjectIdle();
    if(started)
        {
        logProj("+loadAnalysisFiles");
        stopAndWaitForBackgroundComplete();
        loadIncludeMap();
        addTask(ProjectBackgroundItem());
        logProj("-loadAnalysisFiles");
        }
    return started;
    }

void OovProject::loadIncludeMap()
    {
    BuildConfigReader buildConfig;
    std::string incDepsFilePath = buildConfig.getIncDepsFilePath();
    mIncludeMap.read(incDepsFilePath);
    }

void OovProject::stopAndWaitForBackgroundComplete()
    {
    logProj("+stopAndWait");
    // This sets a flag in ThreadedWorkBackgroundQueue that will stop the
    // background thread and abort loops in processAnalysisFiles().
    stopAndWaitForCompletion();
    mBackgroundProc.stopProcess();
    logProj("-stopAndWait");
    }

void OovProject::processAnalysisFiles()
    {
    logProj("+processAnalysisFiles");
    mProjectStatus.mAnalysisStatus |= ProjectStatus::AS_Loading;
    std::vector<std::string> fileNames;
    BuildConfigReader buildConfig;
    OovStatus status = getDirListMatchExt(buildConfig.getAnalysisPath(),
        FilePath(".xmi", FP_File), fileNames);
    if(status.ok())
        {
        int typeIndex = 0;
        OovTaskStatusListenerId taskId = 0;
        if(mStatusListener)
            {
            taskId = mStatusListener->startTask("Loading files.", fileNames.size());
            }
        // The continueProcessingItem is from the ThreadedWorkBackgroundQueue,
        // and is set false when stopAndWaitForCompletion() is called.
        for(size_t i=0; i<fileNames.size() && continueProcessingItem(); i++)
            {
            OovString fileText = "File ";
            fileText.appendInt(i);
            fileText += ": ";
            fileText += fileNames[i];
            if(mStatusListener && !mStatusListener->updateProgressIteration(
                    taskId, i, fileText))
                {
                break;
                }
            File file;
            OovStatus status = file.open(fileNames[i], "r");
            if(status.ok())
                {
                loadXmiFile(file, mModelData, fileNames[i], typeIndex);
                }
            if(status.needReport())
                {
                OovString err = "Unable to read XMI file ";
                err += fileNames[i];
                status.report(ET_Error, err);
                }
            }
    logProj(" processAnalysisFiles - loaded");
        if(continueProcessingItem())
            {
            if(mStatusListener)
                {
                taskId = mStatusListener->startTask("Resolving Model.", 100);
                }
            mModelData.resolveModelIds();
    logProj(" processAnalysisFiles - resolved");
            if(mStatusListener)
                {
                mStatusListener->updateProgressIteration(taskId, 50, nullptr);
                mStatusListener->endTask(taskId);
                }
            }
        }
    if(status.needReport())
        {
        status.report(ET_Error, "Unable to get directory for analysis");
        }
    mProjectStatus.mAnalysisStatus |= ProjectStatus::AS_Loaded;
    logProj("-processAnalysisFiles");
    }

bool OovProject::runSrcManager(OovStringRef const buildConfigName,
        OovStringRef const runStr, eSrcManagerOptions smo)
    {
    bool success = true;
    OovString procPath = Project::getBinDirectory();
    procPath += FilePathMakeExeFilename("oovBuilder");
    OovProcessChildArgs args;
    args.addArg(procPath);
    args.addArg(Project::getProjectDirectory());

    switch(smo)
        {
        case SM_Analyze:
            args.addArg("-mode-analyze");
            break;

        case SM_Build:
            args.addArg("-mode-build");
            args.addArg(makeBuildConfigArgName("-cfg", buildConfigName));
            break;

        case SM_CovInstr:
            args.addArg("-mode-cov-instr");
            args.addArg(makeBuildConfigArgName("-cfg", BuildConfigAnalysis));
            break;

        case  SM_CovBuild:
            args.addArg("-mode-cov-build");
            args.addArg(makeBuildConfigArgName("-cfg", BuildConfigDebug));
            break;

        case SM_CovStats:
            args.addArg("-mode-cov-stats");
            args.addArg(makeBuildConfigArgName("-cfg", BuildConfigDebug));
            break;
        }
    if(success)
        {
        success = mBackgroundProc.startProcess(procPath, args.getArgv(), false);
        }
    return success;
    }

void OovProject::stopSrcManager()
    {
    mBackgroundProc.childProcessKill();
    }

ProjectStatus OovProject::getProjectStatus()
    {
    mProjectStatus.mBackgroundProcIdle = mBackgroundProc.isIdle();
    mProjectStatus.mBackgroundThreadIdle = !isQueueBusy();
    return mProjectStatus;
    }
