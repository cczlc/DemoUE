#pragma once
#include "Definitions.h"
#include "HAL/Platform.h"

enum class EConfigCacheType : uint8
{
	// this type of config cache will write its files to disk during Flush (i.e. GConfig)
	DiskBacked,
	// this type of config cache is temporary and will never write to disk (only load from disk)
	Temporary,
};


// Set of all cached config files.
class FConfigCacheIni
{
public:
	// Basic functions.
	CORE_API FConfigCacheIni(EConfigCacheType Type);

	/** DO NOT USE. This constructor is for internal usage only for hot-reload purposes. */
	CORE_API FConfigCacheIni();

	CORE_API virtual ~FConfigCacheIni();

	/**
	* Disables any file IO by the config cache system
	*/
	CORE_API virtual void DisableFileOperations();

	/**
	* Re-enables file IO by the config cache system
	*/
	CORE_API virtual void EnableFileOperations();

	/**
	 * Returns whether or not file operations are disabled
	 */
	CORE_API virtual bool AreFileOperationsDisabled();

	/**
	 * @return true after after the basic .ini files have been loaded
	 */
	bool IsReadyForUse()
	{
		return bIsReadyForUse;
	}

	/**
	* Prases apart an ini section that contains a list of 1-to-N mappings of strings in the following format
	*	 [PerMapPackages]
	*	 MapName=Map1
	*	 Package=PackageA
	*	 Package=PackageB
	*	 MapName=Map2
	*	 Package=PackageC
	*	 Package=PackageD
	*
	* @param Section Name of section to look in
	* @param KeyOne Key to use for the 1 in the 1-to-N (MapName in the above example)
	* @param KeyN Key to use for the N in the 1-to-N (Package in the above example)
	* @param OutMap Map containing parsed results
	* @param Filename Filename to use to find the section
	*
	* NOTE: The function naming is weird because you can't apparently have an overridden function differnt only by template type params
	*/
	CORE_API virtual void Parse1ToNSectionOfStrings(const TCHAR* Section, const TCHAR* KeyOne, const TCHAR* KeyN, TMap<FString, TArray<FString> >& OutMap, const FString& Filename);

	/**
	* Parses apart an ini section that contains a list of 1-to-N mappings of names in the following format
	*	 [PerMapPackages]
	*	 MapName=Map1
	*	 Package=PackageA
	*	 Package=PackageB
	*	 MapName=Map2
	*	 Package=PackageC
	*	 Package=PackageD
	*
	* @param Section Name of section to look in
	* @param KeyOne Key to use for the 1 in the 1-to-N (MapName in the above example)
	* @param KeyN Key to use for the N in the 1-to-N (Package in the above example)
	* @param OutMap Map containing parsed results
	* @param Filename Filename to use to find the section
	*
	* NOTE: The function naming is weird because you can't apparently have an overridden function differnt only by template type params
	*/
	CORE_API virtual void Parse1ToNSectionOfNames(const TCHAR* Section, const TCHAR* KeyOne, const TCHAR* KeyN, TMap<FName, TArray<FName> >& OutMap, const FString& Filename);

	/**
	 * Finds the in-memory config file for a config cache filename.
	 *
	 * @param A known key like GEngineIni, or the return value of GetConfigFilename
	 *
	 * @return The existing config file or null if it does not exist in memory
	 */
	CORE_API FConfigFile* FindConfigFile(const FString& Filename);

	/**
	 * Finds, loads, or creates the in-memory config file for a config cache filename.
	 *
	 * @param A known key like GEngineIni, or the return value of GetConfigFilename
	 *
	 * @return A new or existing config file
	 */
	CORE_API FConfigFile* Find(const FString& InFilename);

	/**
	 * Reports whether an FConfigFile* is pointing to a config file inside of this
	 * Used for downstream functions to check whether a config file they were passed came from this ConfigCacheIni or from
	 * a different source such as LoadLocalIniFile
	 */
	CORE_API bool ContainsConfigFile(const FConfigFile* ConfigFile) const;

	UE_DEPRECATED(5.0, "CreateIfNotFound is deprecated, please use the overload without this parameter or FindConfigFile")
		CORE_API FConfigFile* Find(const FString& Filename, bool CreateIfNotFound);

	/** Finds Config file that matches the base name such as "Engine" */
	CORE_API FConfigFile* FindConfigFileWithBaseName(FName BaseName);

	FConfigFile& Add(const FString& Filename, const FConfigFile& File)
	{
		return *OtherFiles.Add(Filename, new FConfigFile(File));
	}
	int32 Remove(const FString& Filename)
	{
		delete OtherFiles.FindRef(Filename);
		return OtherFiles.Remove(Filename);
	}
	CORE_API TArray<FString> GetFilenames();


	CORE_API void Flush(bool bRemoveFromCache, const FString& Filename = TEXT(""));

	CORE_API void LoadFile(const FString& InFilename, const FConfigFile* Fallback = NULL, const TCHAR* PlatformString = NULL);
	CORE_API void SetFile(const FString& InFilename, const FConfigFile* NewConfigFile);
	CORE_API void UnloadFile(const FString& Filename);
	CORE_API void Detach(const FString& Filename);

	CORE_API bool GetString(const TCHAR* Section, const TCHAR* Key, FString& Value, const FString& Filename);
	CORE_API bool GetText(const TCHAR* Section, const TCHAR* Key, FText& Value, const FString& Filename);
	CORE_API bool GetSection(const TCHAR* Section, TArray<FString>& Result, const FString& Filename);
	CORE_API bool DoesSectionExist(const TCHAR* Section, const FString& Filename);
	/**
	 * @param Force Whether to create the Section on Filename if it did not exist previously.
	 * @param Const If Const (and not Force), then it will not modify File->Dirty. If not Const (or Force is true), then File->Dirty will be set to true.
	 */
	CORE_API FConfigSection* GetSectionPrivate(const TCHAR* Section, const bool Force, const bool Const, const FString& Filename);
	CORE_API void SetString(const TCHAR* Section, const TCHAR* Key, const TCHAR* Value, const FString& Filename);
	CORE_API void SetText(const TCHAR* Section, const TCHAR* Key, const FText& Value, const FString& Filename);
	CORE_API bool RemoveKey(const TCHAR* Section, const TCHAR* Key, const FString& Filename);
	CORE_API bool EmptySection(const TCHAR* Section, const FString& Filename);
	CORE_API bool EmptySectionsMatchingString(const TCHAR* SectionString, const FString& Filename);

	/**
	 * For a base ini name, gets the config cache filename key that is used by other functions like Find.
	 * This will be the base name for known configs like Engine and the destination filename for others.
	 *
	 * @param IniBaseName Base name of the .ini (Engine, Game, CustomSystem)
	 *
	 * @return Filename key used by other cache functions
	 */
	CORE_API FString GetConfigFilename(const TCHAR* BaseIniName);

	/**
	 * Retrieve a list of all of the config files stored in the cache
	 *
	 * @param ConfigFilenames Out array to receive the list of filenames
	 */
	CORE_API void GetConfigFilenames(TArray<FString>& ConfigFilenames);

	/**
	 * Retrieve the names for all sections contained in the file specified by Filename
	 *
	 * @param	Filename			the file to retrieve section names from
	 * @param	out_SectionNames	will receive the list of section names
	 *
	 * @return	true if the file specified was successfully found;
	 */
	CORE_API bool GetSectionNames(const FString& Filename, TArray<FString>& out_SectionNames);

	/**
	 * Retrieve the names of sections which contain data for the specified PerObjectConfig class.
	 *
	 * @param	Filename			the file to retrieve section names from
	 * @param	SearchClass			the name of the PerObjectConfig class to retrieve sections for.
	 * @param	out_SectionNames	will receive the list of section names that correspond to PerObjectConfig sections of the specified class
	 * @param	MaxResults			the maximum number of section names to retrieve
	 *
	 * @return	true if the file specified was found and it contained at least 1 section for the specified class
	 */
	CORE_API bool GetPerObjectConfigSections(const FString& Filename, const FString& SearchClass, TArray<FString>& out_SectionNames, int32 MaxResults = 1024);

	CORE_API void Exit();

	/**
	 * Prints out the entire config set, or just a single file if an ini is specified
	 *
	 * @param Ar the device to write to
	 * @param IniName An optional ini name to restrict the writing to (Engine or WrangleContent) - meant to be used with "final" .ini files (not Default*)
	 */
	CORE_API void Dump(FOutputDevice& Ar, const TCHAR* IniName = NULL);

	/**
	 * Dumps memory stats for each file in the config cache to the specified archive.
	 *
	 * @param	Ar	the output device to dump the results to
	 */
	CORE_API virtual void ShowMemoryUsage(FOutputDevice& Ar);

	/**
	 * USed to get the max memory usage for the FConfigCacheIni
	 *
	 * @return the amount of memory in byes
	 */
	CORE_API virtual SIZE_T GetMaxMemoryUsage();

	/**
	 * allows to iterate through all key value pairs
	 * @return false:error e.g. Section or Filename not found
	 */
	CORE_API virtual bool ForEachEntry(const FKeyValueSink& Visitor, const TCHAR* Section, const FString& Filename);

	// Derived functions.
	CORE_API FString GetStr
	(
		const TCHAR* Section,
		const TCHAR* Key,
		const FString& Filename
	);
	CORE_API bool GetInt
	(
		const TCHAR* Section,
		const TCHAR* Key,
		int32& Value,
		const FString& Filename
	);
	CORE_API bool GetInt64
	(
		const TCHAR* Section,
		const TCHAR* Key,
		int64& Value,
		const FString& Filename
	);
	CORE_API bool GetFloat
	(
		const TCHAR* Section,
		const TCHAR* Key,
		float& Value,
		const FString& Filename
	);
	CORE_API bool GetDouble
	(
		const TCHAR* Section,
		const TCHAR* Key,
		double& Value,
		const FString& Filename
	);
	CORE_API bool GetBool
	(
		const TCHAR* Section,
		const TCHAR* Key,
		bool& Value,
		const FString& Filename
	);
	CORE_API int32 GetArray
	(
		const TCHAR* Section,
		const TCHAR* Key,
		TArray<FString>& out_Arr,
		const FString& Filename
	);
	/** Loads a "delimited" list of strings
	 * @param Section - Section of the ini file to load from
	 * @param Key - The key in the section of the ini file to load
	 * @param out_Arr - Array to load into
	 * @param Filename - Ini file to load from
	 */
	CORE_API int32 GetSingleLineArray
	(
		const TCHAR* Section,
		const TCHAR* Key,
		TArray<FString>& out_Arr,
		const FString& Filename
	);
	CORE_API bool GetColor
	(
		const TCHAR* Section,
		const TCHAR* Key,
		FColor& Value,
		const FString& Filename
	);
	CORE_API bool GetVector2D(
		const TCHAR* Section,
		const TCHAR* Key,
		FVector2D& Value,
		const FString& Filename);
	CORE_API bool GetVector
	(
		const TCHAR* Section,
		const TCHAR* Key,
		FVector& Value,
		const FString& Filename
	);
	CORE_API bool GetVector4
	(
		const TCHAR* Section,
		const TCHAR* Key,
		FVector4& Value,
		const FString& Filename
	);
	CORE_API bool GetRotator
	(
		const TCHAR* Section,
		const TCHAR* Key,
		FRotator& Value,
		const FString& Filename
	);

	/* Generic versions for use with templates */
	bool GetValue(const TCHAR* Section, const TCHAR* Key, FString& Value, const FString& Filename)
	{
		return GetString(Section, Key, Value, Filename);
	}
	bool GetValue(const TCHAR* Section, const TCHAR* Key, FText& Value, const FString& Filename)
	{
		return GetText(Section, Key, Value, Filename);
	}
	bool GetValue(const TCHAR* Section, const TCHAR* Key, int32& Value, const FString& Filename)
	{
		return GetInt(Section, Key, Value, Filename);
	}
	bool GetValue(const TCHAR* Section, const TCHAR* Key, float& Value, const FString& Filename)
	{
		return GetFloat(Section, Key, Value, Filename);
	}
	bool GetValue(const TCHAR* Section, const TCHAR* Key, bool& Value, const FString& Filename)
	{
		return GetBool(Section, Key, Value, Filename);
	}
	int32 GetValue(const TCHAR* Section, const TCHAR* Key, TArray<FString>& Value, const FString& Filename)
	{
		return GetArray(Section, Key, Value, Filename);
	}

	// Return a config value if found, if not found return default value
	// does not indicate if return value came from config or the default value
	// useful for one-time init of static variables in code locations where config may be queried too often, like :
	//  static int32 bMyConfigValue = GConfig->GetIntOrDefault(Section,Key,DefaultValue,ConfigFilename);
	int32 GetIntOrDefault(const TCHAR* Section, const TCHAR* Key, const int32 DefaultValue, const FString& Filename)
	{
		int32 Value = DefaultValue;
		GetInt(Section, Key, Value, Filename);
		return Value;
	}
	float GetFloatOrDefault(const TCHAR* Section, const TCHAR* Key, const float DefaultValue, const FString& Filename)
	{
		float Value = DefaultValue;
		GetFloat(Section, Key, Value, Filename);
		return Value;
	}
	bool GetBoolOrDefault(const TCHAR* Section, const TCHAR* Key, const bool DefaultValue, const FString& Filename)
	{
		bool Value = DefaultValue;
		GetBool(Section, Key, Value, Filename);
		return Value;
	}
	FString GetStringOrDefault(const TCHAR* Section, const TCHAR* Key, const FString& DefaultValue, const FString& Filename)
	{
		FString Value;
		if (GetString(Section, Key, Value, Filename))
		{
			return Value;
		}
		else
		{
			return DefaultValue;
		}
	}
	FText GetTextOrDefault(const TCHAR* Section, const TCHAR* Key, const FText& DefaultValue, const FString& Filename)
	{
		FText Value;
		if (GetText(Section, Key, Value, Filename))
		{
			return Value;
		}
		else
		{
			return DefaultValue;
		}
	}

	CORE_API void SetInt
	(
		const TCHAR* Section,
		const TCHAR* Key,
		int32					Value,
		const FString& Filename
	);
	CORE_API void SetFloat
	(
		const TCHAR* Section,
		const TCHAR* Key,
		float				Value,
		const FString& Filename
	);
	CORE_API void SetDouble
	(
		const TCHAR* Section,
		const TCHAR* Key,
		double				Value,
		const FString& Filename
	);
	CORE_API void SetBool
	(
		const TCHAR* Section,
		const TCHAR* Key,
		bool				Value,
		const FString& Filename
	);
	CORE_API void SetArray
	(
		const TCHAR* Section,
		const TCHAR* Key,
		const TArray<FString>& Value,
		const FString& Filename
	);
	/** Saves a "delimited" list of strings
	 * @param Section - Section of the ini file to save to
	 * @param Key - The key in the section of the ini file to save
	 * @param out_Arr - Array to save from
	 * @param Filename - Ini file to save to
	 */
	CORE_API void SetSingleLineArray
	(
		const TCHAR* Section,
		const TCHAR* Key,
		const TArray<FString>& In_Arr,
		const FString& Filename
	);
	CORE_API void SetColor
	(
		const TCHAR* Section,
		const TCHAR* Key,
		FColor				Value,
		const FString& Filename
	);
	CORE_API void SetVector2D(
		const TCHAR* Section,
		const TCHAR* Key,
		FVector2D      Value,
		const FString& Filename);
	CORE_API void SetVector
	(
		const TCHAR* Section,
		const TCHAR* Key,
		FVector				Value,
		const FString& Filename
	);
	CORE_API void SetVector4
	(
		const TCHAR* Section,
		const TCHAR* Key,
		const FVector4& Value,
		const FString& Filename
	);
	CORE_API void SetRotator
	(
		const TCHAR* Section,
		const TCHAR* Key,
		FRotator			Value,
		const FString& Filename
	);

	// Static helper functions

	/**
	 * Creates GConfig, loads the standard global ini files (Engine, Editor, etc),
	 * fills out GEngineIni, etc. and marks GConfig as ready for use
	 */
	static CORE_API void InitializeConfigSystem();

	/**
	 * Returns the Custom Config string, which if set will load additional config files from Config/Custom/{CustomConfig}/DefaultX.ini to allow different types of builds.
	 * It can be set from a game Target.cs file with CustomConfig = "Name".
	 * Or in development, it can be overridden with a -CustomConfig=Name command line parameter.
	 */
	static CORE_API const FString& GetCustomConfigString();

	/**
	 * Calculates the name of a dest (generated) .ini file for a given base (ie Engine, Game, etc)
	 *
	 * @param IniBaseName Base name of the .ini (Engine, Game)
	 * @param PlatformName Name of the platform to get the .ini path for (nullptr means to use the current platform)
	 * @param GeneratedConfigDir The base folder that will contain the generated config files.
	 *
	 * @return Standardized .ini filename
	 */
	static CORE_API FString GetDestIniFilename(const TCHAR* BaseIniName, const TCHAR* PlatformName, const TCHAR* GeneratedConfigDir);

	/**
	 * Loads and generates a destination ini file and adds it to GConfig:
	 *   - Looking on commandline for override source/dest .ini filenames
	 *   - Generating the name for the engine to refer to the ini
	 *   - Loading a source .ini file hierarchy
	 *   - Filling out an FConfigFile
	 *   - Save the generated ini
	 *   - Adds the FConfigFile to GConfig
	 *
	 * @param FinalIniFilename The output name of the generated .ini file (in Game\Saved\Config)
	 * @param BaseIniName The "base" ini name, with no extension (ie, Engine, Game, etc)
	 * @param Platform The platform to load the .ini for (if NULL, uses current)
	 * @param bForceReload If true, the destination .in will be regenerated from the source, otherwise this will only process if the dest isn't in GConfig
	 * @param bRequireDefaultIni If true, the Default*.ini file is required to exist when generating the final ini file.
	 * @param bAllowGeneratedIniWhenCooked If true, the engine will attempt to load the generated/user INI file when loading cooked games
	 * @param GeneratedConfigDir The location where generated config files are made.
	 * @return true if the final ini was created successfully.
	 */
	static CORE_API bool LoadGlobalIniFile(FString& FinalIniFilename, const TCHAR* BaseIniName, const TCHAR* Platform = NULL, bool bForceReload = false, bool bRequireDefaultIni = false, bool bAllowGeneratedIniWhenCooked = true, bool bAllowRemoteConfig = true, const TCHAR* GeneratedConfigDir = *FPaths::GeneratedConfigDir(), FConfigCacheIni* ConfigSystem = GConfig);

	/**
	 * Load an ini file directly into an FConfigFile, and nothing is written to GConfig or disk.
	 * The passed in .ini name can be a "base" (Engine, Game) which will be modified by platform and/or commandline override,
	 * or it can be a full ini filename (ie WrangleContent) loaded from the Source config directory
	 *
	 * @param ConfigFile The output object to fill
	 * @param IniName Either a Base ini name (Engine) or a full ini name (WrangleContent). NO PATH OR EXTENSION SHOULD BE USED!
	 * @param bIsBaseIniName true if IniName is a Base name, which can be overridden on commandline, etc.
	 * @param Platform The platform to use for Base ini names, NULL means to use the current platform
	 * @param bForceReload force reload the ini file from disk this is required if you make changes to the ini file not using the config system as the hierarchy cache will not be updated in this case
	 * @return true if the ini file was loaded successfully
	 */
	static CORE_API bool LoadLocalIniFile(FConfigFile& ConfigFile, const TCHAR* IniName, bool bIsBaseIniName, const TCHAR* Platform = NULL, bool bForceReload = false);

	/**
	 * Load an ini file directly into an FConfigFile from the specified config folders, optionally writing to disk.
	 * The passed in .ini name can be a "base" (Engine, Game) which will be modified by platform and/or commandline override,
	 * or it can be a full ini filename (ie WrangleContent) loaded from the Source config directory
	 *
	 * @param ConfigFile The output object to fill
	 * @param IniName Either a Base ini name (Engine) or a full ini name (WrangleContent). NO PATH OR EXTENSION SHOULD BE USED!
	 * @param EngineConfigDir Engine config directory.
	 * @param SourceConfigDir Game config directory.
	 * @param bIsBaseIniName true if IniName is a Base name, which can be overridden on commandline, etc.
	 * @param Platform The platform to use for Base ini names
	 * @param bForceReload force reload the ini file from disk this is required if you make changes to the ini file not using the config system as the hierarchy cache will not be updated in this case
	 * @param bWriteDestIni write out a destination ini file to the Saved folder, only valid if bIsBaseIniName is true
	 * @param bAllowGeneratedIniWhenCooked If true, the engine will attempt to load the generated/user INI file when loading cooked games
	 * @param GeneratedConfigDir The location where generated config files are made.
	 * @return true if the ini file was loaded successfully
	 */
	static CORE_API bool LoadExternalIniFile(FConfigFile& ConfigFile, const TCHAR* IniName, const TCHAR* EngineConfigDir, const TCHAR* SourceConfigDir, bool bIsBaseIniName, const TCHAR* Platform = NULL, bool bForceReload = false, bool bWriteDestIni = false, bool bAllowGeneratedIniWhenCooked = true, const TCHAR* GeneratedConfigDir = *FPaths::GeneratedConfigDir());

	/**
	 * Needs to be called after GConfig is set and LoadCoalescedFile was called.
	 * Loads the state of console variables.
	 * Works even if the variable is registered after the ini file was loaded.
	 */
	static CORE_API void LoadConsoleVariablesFromINI();

	/**
	 * Normalizes file paths to INI files.
	 *
	 * If an INI file is accessed with multiple paths, then we can run into issues where we cache multiple versions
	 * of the file. Specifically, any updates to the file may only be applied to one cached version, and could cause
	 * changes to be lost.
	 *
	 * E.G.
	 *
	 *		// Standard path.
	 *		C:\ProjectDir\Engine\Config\DefaultEngine.ini
	 *
	 *		// Lowercase drive, and an extra slash between ProjectDir and Engine.
	 *		c:\ProjectDir\\Engine\Confg\DefaultEngine.ini
	 *
	 *		// Relative to a project binary.
	 *		..\..\..\ConfigDefaultEngine.ini
	 *
	 *		The paths above could all be used to reference the same ini file (namely, DefaultEngine.ini).
	 *		However, they would end up generating unique entries in the GConfigCache.
	 *		That means any modifications to *one* of the entries would not propagate to the others, and if
	 *		any / all of the ini files are saved, they will stomp changes to the other entries.
	 *
	 *		We can prevent these types of issues by enforcing normalized paths when accessing configs.
	 *
	 *	@param NonNormalizedPath	The path to the INI file we want to access.
	 *	@return A normalized version of the path (may be the same as the input).
	 */
	static CORE_API FString NormalizeConfigIniPath(const FString& NonNormalizedPath);

	/**
	 * This helper function searches the cache before trying to load the ini file using LoadLocalIniFile.
	 * Note that the returned FConfigFile pointer must have the same lifetime as the passed in LocalFile.
	 *
	 * @param LocalFile The output object to fill. If the FConfigFile is found in the cache, this won't be used.
	 * @param IniName Either a Base ini name (Engine) or a full ini name (WrangleContent). NO PATH OR EXTENSION SHOULD BE USED!
	 * @param Platform The platform to use for Base ini names, NULL means to use the current platform
	 * @return FConfigFile Found or loaded FConfigFile
	 */
	static CORE_API FConfigFile* FindOrLoadPlatformConfig(FConfigFile& LocalFile, const TCHAR* IniName, const TCHAR* Platform = NULL);

	/**
	 * Attempts to find the platform config in the cache.
	 *
	 * @param IniName Either a Base ini name (Engine) or a full ini name (WrangleContent). NO PATH OR EXTENSION SHOULD BE USED!
	 * @param Platform The platform to use for Base ini names, NULL means to use the current platform
	 * @return FConfigFile Found FConfigFile
	 */
	static CORE_API FConfigFile* FindPlatformConfig(const TCHAR* IniName, const TCHAR* Platform);

	/**
	 * Save the current config cache state into a file for bootstrapping other processes.
	 */
	CORE_API void SaveCurrentStateForBootstrap(const TCHAR* Filename);

	CORE_API void Serialize(FArchive& Ar);


	struct FKnownConfigFiles
	{
		FKnownConfigFiles();

		// Write out this for binary config serialization
		friend FArchive& operator<<(FArchive& Ar, FKnownConfigFiles& Names);

		// setup GEngineIni based on this structure's values
		void SetGlobalIniStringsFromMembers();

		// given an name ("Engine") return the FConfigFile for it
		const FConfigFile* GetFile(FName Name);
		// given an name ("Engine") return the modifiable FConfigFile for it
		FConfigFile* GetMutableFile(FName Name);

		// get the disk-based filename for the given known ini name
		const FString& GetFilename(FName Name);


		// create the list of members for the known inis (Engine, Game, etc) See the top of this file for the list
		struct FKnownConfigFile
		{
			FName IniName;
			FString IniPath;
			FConfigFile IniFile;
		};

		// array of all known filesd
		FKnownConfigFile Files[(uint8)EKnownIniFile::NumKnownFiles];
	};

	/**
	 * Load the standard (used on all platforms) ini files, like Engine, Input, etc
	 *
	 * @param FConfigContext The loading context that controls the destination of the loaded ini files
	 *
	 * return True if the engine ini was loaded
	 */
	static CORE_API bool InitializeKnownConfigFiles(FConfigContext& Context);

	/**
	 * Returns true if the given name is one of the known configs, where the matching G****Ini property is going to match the
	 * base name ("Engine" returns true, which means GEngineIni's value is just "Engine")
	 */
	CORE_API bool IsKnownConfigName(FName ConfigName);

	/**
	 * Create GConfig from a saved file
	 */
	static CORE_API bool CreateGConfigFromSaved(const TCHAR* Filename);

	/**
	 * Retrieve the fully processed ini system for another platform. The editor will start loading these
	 * in the background on startup
	 */
	static CORE_API FConfigCacheIni* ForPlatform(FName PlatformName);

	/**
	 * Wipe all cached platform configs. Next ForPlatform call will load on-demand the platform configs
	 */
	static CORE_API void ClearOtherPlatformConfigs();

private:
#if WITH_EDITOR
	/** We only auto-initialize other platform configs in the editor to not slow down programs like ShaderCOmpileWorker */
	static CORE_API void AsyncInitializeConfigForPlatforms();
#endif

	/** Serialize a bootstrapping state into or from an archive */
	CORE_API void SerializeStateForBootstrap_Impl(FArchive& Ar);

	/** true if file operations should not be performed */
	bool bAreFileOperationsDisabled;

	/** true after the base .ini files have been loaded, and GConfig is generally "ready for use" */
	bool bIsReadyForUse;

	/** The type of the cache (basically, do we call Flush in the destructor) */
	EConfigCacheType Type;

	/** The filenames for the known files in this config */
	FKnownConfigFiles KnownFiles;

	TMap<FString, FConfigFile*> OtherFiles;

	friend FConfigContext;
};