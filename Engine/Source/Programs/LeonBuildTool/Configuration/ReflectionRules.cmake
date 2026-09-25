# LeonBuildTool reflection rules (UnrealBuildTool: the UnrealHeaderTool step of UEBuildModuleCPP).
#
# A module is reflected when one of its headers (Public/, Classes/ and Private/ of the module and its platform
# extensions; the whole folder for flat modules) has `#include "<Name>.generated.h"`. For each reflected unit:
#   - <tree>/Inc/<Module>/<Unit>.lhtmanifest lists the headers, the module, its API macro, the output folder and the
#     type indexes of the reflected modules it depends on (format: LeonHeaderTool/Private/Manifest.h);
#   - a custom command runs LeonHeaderTool when a header, the manifest, the tool or a dependency's type index changes.
#     Its output is <Unit>.lhtstamp; the generated files are byproducts that LeonHeaderTool only rewrites when their
#     content changes, so ninja recompiles nothing when the reflection data did not change;
#   - <Header>.gen.cpp and <Unit>.init.gen.cpp are compiled into the module and <tree>/Inc/<Module> becomes a PUBLIC
#     include path;
#   - the target's statically linked module table points RegisterReflection at RegisterReflection_<Module>.
# Units: "Module" (the module library, or the executable for a launch module) and "Tests" (<Module>/Private/Tests/**.h,
# only for targets with COLLECT_AUTOMATION_TESTS, compiled into the executable, RegisterReflection_<Module>_Tests).
#
# Header lists come from CONFIGURE_DEPENDS globs, like sources: adding a header reconfigures the tree. When a header
# of a reflected unit gains its first .generated.h include, LeonHeaderTool stops with an error and touches
# <Unit>.lhtreconfigure (a configure dependency), so the next build reconfigures and reflects it. In a module with no
# reflected header yet nothing runs LeonHeaderTool: touch its .Build.cmake (or use -Mode=Rebuild) after adding the
# first include.

# <Root>/**.h minus foreign platform folders (and minus <Root>/Tests/ when ExcludeTests).
function(_leon_reflection_glob Root ExcludeTests OutVar)
	set(Found)
	if(IS_DIRECTORY "${Root}")
		file(GLOB_RECURSE Found CONFIGURE_DEPENDS "${Root}/*.h")
		if(ExcludeTests)
			list(FILTER Found EXCLUDE REGEX "^${Root}/Tests/")
		endif()
		_leon_filter_platform_folders("${Root}" "${Found}" Found)
		list(SORT Found)
	endif()
	set(${OutVar} "${Found}" PARENT_SCOPE)
endfunction()

# Every header of a unit as "<path>|<include path>", the include path being relative to the folder the module
# includes it from (Public/, Classes/, Private/ or the flat module's folder; tests: "Tests/Foo.h" from Private/).
function(_leon_reflection_unit_headers Name Unit OutVar)
	leon_module_get(${Name} DIR Dir)
	_leon_module_ext_dirs(${Name} ExtDirs)
	_leon_module_is_flat(${Name} Flat)
	set(Entries)
	if(Unit STREQUAL "Tests")
		set(TestRoots)
		if(Flat)
			list(APPEND TestRoots "${Dir}|${Dir}/Tests")
		else()
			foreach(Root IN ITEMS "${Dir}" LISTS ExtDirs)
				list(APPEND TestRoots "${Root}/Private|${Root}/Private/Tests")
			endforeach()
		endif()
		foreach(Pair IN LISTS TestRoots)
			string(REPLACE "|" ";" Pair "${Pair}")
			list(GET Pair 0 IncludeRoot)
			list(GET Pair 1 GlobRoot)
			_leon_reflection_glob("${GlobRoot}" FALSE Headers)
			foreach(Header IN LISTS Headers)
				file(RELATIVE_PATH Relative "${IncludeRoot}" "${Header}")
				list(APPEND Entries "${Header}|${Relative}")
			endforeach()
		endforeach()
	else()
		set(Roots)
		if(Flat)
			list(APPEND Roots "${Dir}")
		endif()
		foreach(Root IN ITEMS "${Dir}" LISTS ExtDirs)
			foreach(Sub Public Classes Private)
				list(APPEND Roots "${Root}/${Sub}")
			endforeach()
		endforeach()
		foreach(Root IN LISTS Roots)
			_leon_reflection_glob("${Root}" TRUE Headers)
			foreach(Header IN LISTS Headers)
				file(RELATIVE_PATH Relative "${Root}" "${Header}")
				list(APPEND Entries "${Header}|${Relative}")
			endforeach()
		endforeach()
	endif()
	set(${OutVar} "${Entries}" PARENT_SCOPE)
endfunction()

# Reflected modules reachable through public / private dependencies (circular edges are skipped: two reflected
# modules cannot each wait for the other's type index).
function(_leon_reflection_dependency_closure Name OutVar)
	set(Visited)
	leon_module_effective(${Name} PUBLIC_DEPENDENCIES Public)
	leon_module_effective(${Name} PRIVATE_DEPENDENCIES Private)
	set(Queue ${Public} ${Private})
	while(Queue)
		list(POP_FRONT Queue Dep)
		if(Dep IN_LIST Visited OR Dep STREQUAL Name)
			continue()
		endif()
		list(APPEND Visited ${Dep})
		leon_module_get(${Dep} TYPE DepType)
		if(DepType STREQUAL "External")
			continue()
		endif()
		leon_module_effective(${Dep} PUBLIC_DEPENDENCIES DepPublic)
		leon_module_effective(${Dep} PRIVATE_DEPENDENCIES DepPrivate)
		list(APPEND Queue ${DepPublic} ${DepPrivate})
	endwhile()
	set(${OutVar} "${Visited}" PARENT_SCOPE)
endfunction()

# Sets up the LeonHeaderTool step of one unit (once per build tree) and returns what to compile:
#   OutSources    <Header>.gen.cpp files, <Unit>.init.gen.cpp and the stamp (empty when the unit is not reflected)
#   OutIncludeDir <tree>/Inc/<Module> (empty when the unit is not reflected)
# Also sets the global property LEON_REFLECTION_<Name>_<Unit>_FUNCTION to the registration function.
function(leon_module_reflection Name Unit OutSources OutIncludeDir)
	set(Key "LEON_REFLECTION_${Name}_${Unit}")
	get_property(Done GLOBAL PROPERTY ${Key}_DONE)
	if(NOT Done)
		set_property(GLOBAL PROPERTY ${Key}_DONE TRUE)
		_leon_reflection_setup(${Name} ${Unit})
	endif()
	get_property(Sources GLOBAL PROPERTY ${Key}_SOURCES)
	get_property(IncludeDir GLOBAL PROPERTY ${Key}_INCLUDE_DIR)
	set(${OutSources} "${Sources}" PARENT_SCOPE)
	set(${OutIncludeDir} "${IncludeDir}" PARENT_SCOPE)
endfunction()

function(_leon_reflection_setup Name Unit)
	set(Key "LEON_REFLECTION_${Name}_${Unit}")
	leon_module_get(${Name} TYPE Type)
	if(Type STREQUAL "External")
		return()
	endif()

	# Reflected headers: the ones that include their .generated.h. The others are candidates LeonHeaderTool checks for
	# a new include (see the header comment).
	_leon_reflection_unit_headers(${Name} ${Unit} Entries)
	set(Headers)
	set(Candidates)
	set(HeaderPaths)
	foreach(Entry IN LISTS Entries)
		string(REPLACE "|" ";" Fields "${Entry}")
		list(GET Fields 0 Header)
		file(STRINGS "${Header}" Includes LIMIT_COUNT 1
			REGEX "^[ \t]*#[ \t]*include[ \t]*[\"<][^\">]*\\.generated\\.h[\">]")
		if(Includes)
			list(APPEND Headers "${Entry}")
			list(APPEND HeaderPaths "${Header}")
		else()
			list(APPEND Candidates "${Header}")
		endif()
	endforeach()
	if(NOT Headers)
		return()
	endif()
	if(NOT LEON_HEADER_TOOL)
		message(FATAL_ERROR "LeonBuildTool: module '${Name}' is reflected but LEON_HEADER_TOOL is not set; configure "
			"through LeonBuildTool (Engine/Build/BatchFiles/Build.bat), which builds LeonHeaderTool first")
	endif()

	set(OutputDir "${CMAKE_BINARY_DIR}/Inc/${Name}")
	if(Unit STREQUAL "Tests")
		set(UnitName "${Name}.Tests")
		set(Function "RegisterReflection_${Name}_Tests")
	else()
		set(UnitName "${Name}")
		set(Function "RegisterReflection_${Name}")
	endif()

	# Type indexes of the reflected modules this one can see (and, for tests, of the module itself).
	set(DependencyIndexes)
	_leon_reflection_dependency_closure(${Name} Closure)
	if(Unit STREQUAL "Tests")
		list(PREPEND Closure ${Name})
	endif()
	set(DefinePackage 1)
	foreach(Dep IN LISTS Closure)
		leon_module_reflection(${Dep} Module DepSources DepIncludeDir)
		if(DepSources)
			list(APPEND DependencyIndexes "${DepIncludeDir}/${Dep}.lhttypes")
			if(Dep STREQUAL Name)
				set(DefinePackage 0)
			endif()
		endif()
	endforeach()

	# Generated file names are the header names: two reflected headers of a module cannot share one.
	set(BaseNames)
	if(Unit STREQUAL "Tests")
		get_property(BaseNames GLOBAL PROPERTY LEON_REFLECTION_${Name}_Module_BASE_NAMES)
	endif()
	set(Outputs)
	set(Sources)
	foreach(Header IN LISTS HeaderPaths)
		get_filename_component(Base "${Header}" NAME_WE)
		if(Base IN_LIST BaseNames)
			message(FATAL_ERROR "LeonBuildTool: module '${Name}' has two reflected headers named ${Base}.h "
				"(generated files are named after the header)")
		endif()
		list(APPEND BaseNames ${Base})
		list(APPEND Outputs "${OutputDir}/${Base}.generated.h" "${OutputDir}/${Base}.gen.cpp")
		list(APPEND Sources "${OutputDir}/${Base}.gen.cpp")
	endforeach()
	set_property(GLOBAL PROPERTY ${Key}_BASE_NAMES "${BaseNames}")
	list(APPEND Outputs "${OutputDir}/${UnitName}.init.gen.cpp" "${OutputDir}/${UnitName}.lhttypes")
	list(APPEND Sources "${OutputDir}/${UnitName}.init.gen.cpp")

	# FileId root: the repository, or the folder holding a project that lives outside it.
	set(RootDir "${LEON_ROOT_DIR}")
	leon_module_get(${Name} DIR ModuleDir)
	string(FIND "${ModuleDir}/" "${LEON_ROOT_DIR}/" RootPos)
	if(NOT RootPos EQUAL 0 AND LEON_PROJECT_DIR)
		get_filename_component(RootDir "${LEON_PROJECT_DIR}" DIRECTORY)
	endif()

	string(TOUPPER "${Name}_API" Api)
	set(Manifest "${OutputDir}/${UnitName}.lhtmanifest")
	set(Stamp "${OutputDir}/${UnitName}.lhtstamp")
	set(ReconfigureStamp "${OutputDir}/${UnitName}.lhtreconfigure")
	set(Content "# LeonHeaderTool manifest for ${UnitName} (generated by LeonBuildTool; do not edit).\n")
	string(APPEND Content "Module=${Name}\nApi=${Api}\nPackage=/Script/${Name}\nRootDir=${RootDir}\n")
	string(APPEND Content "OutputDir=${OutputDir}\nInitFile=${UnitName}.init.gen.cpp\nRegisterFunction=${Function}\n")
	string(APPEND Content "DefinePackage=${DefinePackage}\nTypeIndex=${UnitName}.lhttypes\n")
	string(APPEND Content "Stamp=${UnitName}.lhtstamp\nReconfigureStamp=${ReconfigureStamp}\n")
	foreach(Index IN LISTS DependencyIndexes)
		string(APPEND Content "Dependency=${Index}\n")
	endforeach()
	foreach(Entry IN LISTS Headers)
		string(APPEND Content "Header=${Entry}\n")
	endforeach()
	foreach(Candidate IN LISTS Candidates)
		string(APPEND Content "Candidate=${Candidate}\n")
	endforeach()
	file(WRITE "${Manifest}.tmp" "${Content}")
	configure_file("${Manifest}.tmp" "${Manifest}" COPYONLY)
	if(NOT EXISTS "${ReconfigureStamp}")
		file(WRITE "${ReconfigureStamp}" "Touched by LeonHeaderTool when a header gains its first .generated.h include.\n")
	endif()
	set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${ReconfigureStamp}")

	add_custom_command(
		OUTPUT "${Stamp}"
		BYPRODUCTS ${Outputs}
		COMMAND "${LEON_HEADER_TOOL}" "${Manifest}"
		DEPENDS "${LEON_HEADER_TOOL}" "${Manifest}" ${HeaderPaths} ${Candidates} ${DependencyIndexes}
		COMMENT "LeonHeaderTool ${UnitName}"
		VERBATIM)
	list(APPEND Sources "${Stamp}")
	if(Unit STREQUAL "Module")
		# The generation step as a target of its own, for modules that include these headers without a link edge the
		# build orders by (CIRCULAR_DEPENDENCIES: UMG's circular edge to Engine waits for Engine's reflected headers).
		add_custom_target(LeonHeaderTool.${Name} DEPENDS "${Stamp}")
	endif()

	set_property(GLOBAL PROPERTY ${Key}_SOURCES "${Sources}")
	set_property(GLOBAL PROPERTY ${Key}_INCLUDE_DIR "${OutputDir}")
	set_property(GLOBAL PROPERTY ${Key}_FUNCTION "${Function}")
	message(STATUS "LeonBuildTool: ${UnitName} is reflected (LeonHeaderTool, ${OutputDir})")
endfunction()
