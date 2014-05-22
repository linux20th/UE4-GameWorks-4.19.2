// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Text;

namespace UnrealBuildTool
{
	/// <summary>
	/// Configuration class for LinkEnvironment
	/// </summary>
	public class LinkEnvironmentConfiguration
	{
		/** The directory to put the non-executable files in (PDBs, import library, etc) */
		public string OutputDirectory;

		/** Intermediate file directory */
		public string IntermediateDirectory;

		/** The directory to shadow source files in for syncing to remote compile servers */
		public string LocalShadowDirectory = null;

		/** The file path for the executable file that is output by the linker. */
		public string OutputFilePath;

		/** The platform that is being linked for. */
		public CPPTargetPlatform TargetPlatform;

		/** The architecture that is being linked (empty string by default) */
		public string TargetArchitecture = "";

		/** The configuration that is being linked for. */
		public CPPTargetConfiguration TargetConfiguration;

		/** A list of the paths used to find libraries. */
		public List<string> LibraryPaths = new List<string>();

		/** A list of libraries to exclude from linking. */
		public List<string> ExcludedLibraries = new List<string>();

		/** A list of additional libraries to link in. */
		public List<string> AdditionalLibraries = new List<string>();

		/** A list of additional frameworks to link in. */
		public List<UEBuildFramework> AdditionalFrameworks = new List<UEBuildFramework>();

		/** For builds that execute on a remote machine (e.g. iPhone), this list contains additional files that
			need to be copied over in order for the app to link successfully.  Source/header files and PCHs are
			automatically copied.  Usually this is simply a list of precompiled third party library dependencies. */
		public List<string> AdditionalShadowFiles = new List<string>();

		/** The iOS/Mac frameworks to link in */
		public List<string> Frameworks = new List<string>();
		public List<string> WeakFrameworks = new List<string>();

		/**
		 * A list of the dynamically linked libraries that shouldn't be loaded until they are first called
		 * into.
		 */
		public List<string> DelayLoadDLLs = new List<string>();

		/** Additional arguments to pass to the linker. */
		public string AdditionalArguments = "";

		/** True if debug info should be created. */
		public bool bCreateDebugInfo = true;

		/** Whether the CLR (Common Language Runtime) support should be enabled for C++ targets (C++/CLI). */
		public CPPCLRMode CLRMode = CPPCLRMode.CLRDisabled;

		/** True if we're compiling .cpp files that will go into a library (.lib file) */
		public bool bIsBuildingLibrary = false;

		/** True if we're compiling a DLL */
		public bool bIsBuildingDLL = false;

		/** True if this is a console application that's being build */
		public bool bIsBuildingConsoleApplication = false;

		/** If bIsBuildingConsoleApplication is false and this is true, additional console app exe will be built. */
		public bool bBuildAdditionalConsoleApplication = true;

		/** If set, overrides the program entry function on Windows platform.  This is used by the base UE4
		    program so we can link in either command-line mode or windowed mode without having to recompile the Launch module. */
		public string WindowsEntryPointOverride = String.Empty;

		/** True if we're building a EXE/DLL target with an import library, and that library is needed by a dependency that
		    we're directly dependent on. */
		public bool bIsCrossReferenced = false;

		/** True if we should include dependent libraries when building a static library */
		public bool bIncludeDependentLibrariesInLibrary = false;

		/** True if the application we're linking has any exports, and we should be expecting the linker to
		    generate a .lib and/or .exp file along with the target output file */
		public bool bHasExports = true;

		/** True if we're building a .NET assembly (e.g. C# project) */
		public bool bIsBuildingDotNetAssembly = false;

		/** Default constructor. */
		public LinkEnvironmentConfiguration()
		{
		}

		/** Copy constructor. */
		public LinkEnvironmentConfiguration(LinkEnvironmentConfiguration InCopyEnvironment)
		{
			OutputDirectory = InCopyEnvironment.OutputDirectory;
			IntermediateDirectory = InCopyEnvironment.IntermediateDirectory;
			LocalShadowDirectory = InCopyEnvironment.LocalShadowDirectory;
			OutputFilePath = InCopyEnvironment.OutputFilePath;
			TargetPlatform = InCopyEnvironment.TargetPlatform;
			TargetConfiguration = InCopyEnvironment.TargetConfiguration;
			TargetArchitecture = InCopyEnvironment.TargetArchitecture;
			LibraryPaths.AddRange(InCopyEnvironment.LibraryPaths);
			ExcludedLibraries.AddRange(InCopyEnvironment.ExcludedLibraries);
			AdditionalLibraries.AddRange(InCopyEnvironment.AdditionalLibraries);
			Frameworks.AddRange(InCopyEnvironment.Frameworks);
			AdditionalShadowFiles.AddRange( InCopyEnvironment.AdditionalShadowFiles );
			AdditionalFrameworks.AddRange(InCopyEnvironment.AdditionalFrameworks);
			WeakFrameworks.AddRange(InCopyEnvironment.WeakFrameworks);
			DelayLoadDLLs.AddRange(InCopyEnvironment.DelayLoadDLLs);
			AdditionalArguments = InCopyEnvironment.AdditionalArguments;
			bCreateDebugInfo = InCopyEnvironment.bCreateDebugInfo;
			CLRMode = InCopyEnvironment.CLRMode;
			bIsBuildingLibrary = InCopyEnvironment.bIsBuildingLibrary;
			bIsBuildingDLL = InCopyEnvironment.bIsBuildingDLL;
			bIsBuildingConsoleApplication = InCopyEnvironment.bIsBuildingConsoleApplication;
			bBuildAdditionalConsoleApplication = InCopyEnvironment.bBuildAdditionalConsoleApplication;
			WindowsEntryPointOverride = InCopyEnvironment.WindowsEntryPointOverride;
			bIsCrossReferenced = InCopyEnvironment.bIsCrossReferenced;
			bHasExports = InCopyEnvironment.bHasExports;
			bIsBuildingDotNetAssembly = InCopyEnvironment.bIsBuildingDotNetAssembly;
		}

		/** Whether this configuration is suitable to produce additional console app. */
		public bool CanProduceAdditionalConsoleApp
		{
			get
			{
				return TargetPlatform == CPPTargetPlatform.Win64 &&
						bIsBuildingConsoleApplication == false &&
						bBuildAdditionalConsoleApplication == true &&
						bIsBuildingDLL == false &&
						(TargetConfiguration == CPPTargetConfiguration.Debug || TargetConfiguration == CPPTargetConfiguration.Development);
			}
		}
	}

	/** Encapsulates the environment that is used to link object files. */
	public class LinkEnvironment
	{
		/** A list of the object files to be linked. */
		public List<FileItem> InputFiles = new List<FileItem>();

		/** A list of dependent static or import libraries that need to be linked. */
		public List<FileItem> InputLibraries = new List<FileItem>();

		/** The LinkEnvironmentConfiguration. */
		public LinkEnvironmentConfiguration Config = new LinkEnvironmentConfiguration();

		/** Default constructor. */
		public LinkEnvironment()
		{
		}

		/** Copy constructor. */
		protected LinkEnvironment(LinkEnvironment InCopyEnvironment)
		{
			InputFiles.AddRange(InCopyEnvironment.InputFiles);
			InputLibraries.AddRange(InCopyEnvironment.InputLibraries);

			Config = new LinkEnvironmentConfiguration(InCopyEnvironment.Config);
		}

		/** Links the input files into an executable. */
		public FileItem LinkExecutable( bool bBuildImportLibraryOnly )
		{
			return UEToolChain.GetPlatformToolChain(Config.TargetPlatform).LinkFiles(this, bBuildImportLibraryOnly);
		}

		/// <summary>
		/// Performs a deep copy of this LinkEnvironment object.
		/// </summary>
		/// <returns>Copied new LinkEnvironment object.</returns>
		public virtual LinkEnvironment DeepCopy ()
		{
			return new LinkEnvironment(this);
		}
	}
}
