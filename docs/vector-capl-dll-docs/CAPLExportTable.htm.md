---
meta-msapplication-config: ../../../../../Skins/Favicons/browserconfig.xml
meta-viewport: width=device-width, initial-scale=1.0
title: Description of the CAPL Export Table
---

[Open topic with navigation](../../../../../CANoe.htm#Topics/ProgrammingInterfaces/CAPL/CAPLDLL/CAPLExportTable.htm)

[CAPL Introduction](../CAPLIntroduction.htm) Â» [CAPL DLL](CAPLIncludeWindowsDLL.htm) Â» Export Table

# Description of the CAPL Export Table

 

[Valid for](../../../Shared/FeatureAvailability.htm "Feature availabilty for your product"): CANoe DE â¢ CANoe MedTech DE â¢ CANoe4SW:lite DE

Functions that have been created can be exported into CAPL code with the aid of a table function (CAPL_DLL_INFO_LIST).  

The first row of the table contains version information. This row must be defined in the following manner:

{CDLL_VERSION_NAME, (CAPL_FARCALL)CDLL_VERSION, "", "", CAPL_DLL_CDECL, 0xabcd, CDLL_EXPORT },

## [ClosedInformation and Examples on the Functions](javascript:void(0))

The description of a function for export has the following structure:

| Column | Description                          | Comment                                                                                                                                                                  |
| ------ | ------------------------------------ | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| 1      | Name of the function for export      | â                                                                                                                                                                      |
| 2      | Address of the function in the DLL   | â                                                                                                                                                                      |
| 3      | Name of the function category        | â                                                                                                                                                                      |
| 4      | Text that describes the function     | â                                                                                                                                                                      |
| 5      | Value type of the return parameter   | â                                                                                                                                                                      |
| 6      | Number of function parameters        | When using<br>CAPL\_DLL\_INFO3: up to 10 parametersCAPL\_DLL\_INFO4: up to 64 parameters                                                                                 |
| 7      | Value type of the function parameter | â                                                                                                                                                                      |
| 8      | Dimensions of the function parameter | In order of the function parameters the parameters in octal format are characterized as follows:<br>\000: Scalar   \001: 1-dimensional Array   \002: 2-dimensional Array |
| 9      | Name of the function parameter       | â                                                                                                                                                                      |

| ![Example](../../../../Resources/vImages/vExample.png "Example") | Example: Using the Export Table<br>void CAPLEXPORT far CAPLPASCAL appPut(unsigned long x   {   data = x;   }    {"dllPut", (CAPL\_FARCALL)appPut, "CAPL\_DLL","This function will save data from CAPL to DLL memory",'V', 1, "D", "\000", {"x"}},    | Element                      | Description                                          | | ---------------------------- | ---------------------------------------------------- | | Function name                | dllPut                                               | | Function address             | appPut                                               | | Function category            | CAPL\_DLL                                            | | Text                         | This function will save data from CAPL to DLL memory | | Return value                 | V (void)                                             | | Number of function parameter | 1                                                    | | Parameter types              | "D" (dword)                                          | | Parameter dimensions         | "\000" (scalar)                                      | | Parameter name               | "x"                                                  | |
| ---------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |

| ![Example](../../../../Resources/vImages/vExample.png "Example") | Example: Using Array Parameters<br>void CAPLEXPORT far CAPLPASCAL appGetDataTwoPars(unsigned long numberBytes,   unsigned char dataBlock[] )   {    unsigned int i;    for (i = 0; i < numberBytes; i++) {    dataBlock[i] = dlldata[i];    }   }    {"dllGetDataTwoPars", (CAPL\_FARCALL)appGetDataTwoPars, "CAPL\_DLL", "This function will get two datas from DLL into CAPL memory", 'V', 2, "DB", "\000\001", {"noOfBytes", "datablock"}},    | Element                      | Description                                                | | ---------------------------- | ---------------------------------------------------------- | | Function name                | dllGetDataTwoPars                                          | | Function address             | appGetDataTwoPars                                          | | Function category            | CAPL\_DLL                                                  | | Text                         | This function will get two datas from DLL into CAPL memory | | Return value                 | V (void)                                                   | | Number of function parameter | 2                                                          | | Parameter types              | "DB" (dword and byte)                                      | | Parameter dimensions         | "\000\001" (scalar and 1-dimensional)                      | | Parameter name               | "noOfBytes" and "datablock"                                | |
| ---------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |

## [ClosedCharacters for Parameter Types and Return Value Types](javascript:void(0))

The following types can be used as parameter values and return values:

| Character | Value Type   in CAPL | Value Type   in C/C++ | Value Type   in C99/C++11 | [vmodule](../../../DescriptionFiles/VModule/VModule.htm) | Comment                                              |
| --------- | -------------------- | --------------------- | ------------------------- | -------------------------------------------------------- | ---------------------------------------------------- |
| V         | void                 | void                  | void                      | void                                                     | â                                                  |
| C         | char                 | char                  | char                      | char                                                     | only for arrays, i.e. with parameter dimensions != 0 |
| B         | byte                 | unsigned char         | uint8\_t                  | byte                                                     | only for arrays, i.e. with parameter dimensions != 0 |
| I         | int                  | short                 | int16\_t                  | int                                                      | only for arrays, i.e. with parameter dimensions != 0 |
| W         | word                 | unsigned short        | uint16\_t                 | word                                                     | only for arrays, i.e. with parameter dimensions != 0 |
| L         | long                 | long                  | int32\_t                  | long                                                     | â                                                  |
| D         | dword                | unsigned long         | uint32\_t                 | dword                                                    | â                                                  |
| 6         | int64                | long long             | int64\_t                  | int64                                                    | â                                                  |
| U         | qword                | unsigned long long    | uint64\_t                 | qword                                                    | â                                                  |
| F         | float                | double                | double                    | float                                                    | â                                                  |

| ![Note](../../../../Resources/vImages/vInfo.png "Note") | Note<br>Reference parameters can be used also in functions that were exported from CAPL DLLs. Therefore has to be declared the parameter of the function as pointer on the real data type. After that subtract 128 from the character that represents the data type in the CAPL export table. Then the function receives the address of the passed variable as value of the parameter.   Arrays cannot be passed as reference parameters<br>The Value Type column in C/C++ applies to the Visual C++ compiler from Microsoft on Windows and is not suitable for the GCC and Clang compilers on Linux. For portable code please use the type specifications from the newer standards C99 or C++11. |
| ------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |

| ![Example](../../../../Resources/vImages/vExample.png "Example") | Example Using Reference Parameters<br>void CAPLEXPORT far CAPLPASCAL appSum(long i, long j, long\* s)    {    \*s = i + j;    }      {"sum", (CAPL\_FARCALL) appSum, "CAPL\_DLL", "Sum via     reference parameter", 'V', 3, {'L', 'L', 'L' - 128}, "", {"i", "j", "s"} },    | Element                      | Description                             | | ---------------------------- | --------------------------------------- | | Function name                | sum                                     | | Function address             | appSum                                  | | Function category            | CAPL\_DLL                               | | Text                         | Sum via reference parameter             | | Return value                 | V (void)                                | | Number of function parameter | 3                                       | | Parameter types              | 'L', 'L', 'L' - 128 (long, long, long&) | | Parameter dimensions         | "" (all scalar)                         | | Parameter name               | "i", "j", "s"                           | |
| ---------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |

## [ClosedExport of the DLL Function](javascript:void(0))

The export of the DLL functions can be done in two ways:

* assignment

CAPLEXPORT CAPL_DLL_INFO3 far \* caplDllTable3 = table;

* function

unsigned long CAPLEXPORT __cdecl caplDllGetTable3(void(  
{  
return (unsigned long)CAPL_DLL_INFO_LIST3;  
}

| ![Note](../../../../Resources/vImages/vInfo.png "Note") | Note on CAPL DLLs<br>If functions of this DLL are called in the realtime branch ([Simulation Setup](../../../CANoeCANalyzer/Windows/SimulationSetup/SimulationSetupWindow.htm)) the following must be observed:<br>File accesses and other blocking calls are prohibited!Dynamic memory management is not recommended. This means nonew, new[], delete, delete[], malloc, free, etc. Instead it is better to reserve memory statically before the measurement start. |
| ------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |

## [ClosedCAPL Callback Functions](javascript:void(0))

CAPL callback functions may be called independently by a CAPL DLL during a measurement run.  
The interface for use of CAPL callback functions is described in the header files VIA_CDLL.h and VIA.h.  
You will find the header files in this folder: [Open Folder](javascript:startCANoeLauncher('"SAMPLES:\\Programming\\CAPLdll"'))

### Classes and Functions of VIA_CDLL.h

* VIACLIENT(void) VIARegisterCDLL (VIACapl\* service)  
This function is used to initialize the CAPL DLL. It is executed when the register-CAPLDLL(); function is called from a CAPL program.  
Since the DLL may be used simultaneously by multiple CAPL nodes, there is a need for clear differentiation in the CAPL DLL. To achieve this the registerCAPLDLL(); function returns a handle to the CAPL node.
* Methods of the Class VIACapl
* VIASTDDECL GetCaplHandle (uint32\* handle)  
GetCaplHandle determines the handle that is returned to the CAPL node by the registerCAPLDLL(); function.  
This handle must be passed from the CAPL node to the CAPL DLL with each function call.

* VIASTDDECL GetCaplFunction(VIACaplFunction\*\* caplfct, const char\* functionName)  
This function creates a handle for the specified CAPL callback function. The handle must be given with each call of a callback function, and it remains valid up to the end of the measurement or until the ReleaseCaplFunction function is called.

* VIASTDDECL ReleaseCaplFunction(VIACaplFunction\* caplfct)  
This function call is used to release the acquired CAPL callback function handle (see function GetCaplFunction).  
The release should be executed for each CAPL callback function at the end of the measurement.

* VIASTDDECL GetVersion (int32\* major, int32\* minor)  
Returns the version of the interface object.

### The Class VIACaplFunction (VIA.h)

The VIACaplFunction class offers various methods for accessing CAPL functions:

* VIASTDDECL ParamSize (int32\* size)  
Returns the total number of bytes of all parameters of this CAPL function in size.

* VIASTDDECL ParamCount (int32\* size)  
Returns the number of parameters in the signature of this CAPL function in size.

* VIASTDDECL ParamType (char\* type, int32 nth)  
Returns the type of the nth specified parameter (0,1,2,â¦) in the signature of this function in type.

* VIASTDDECL ResultType (char\* type)  
Returns the type of the return value in type.

* VIASTDDECL Call (uint32\* result, void\* params) =0;  
Calls the CAPL function with the specified parameters.  
Params is a pointer to the parameter list.  
Result contains the return value of the CAPL function as uint32.

* VIASTDDECL CallReturnsDouble (double\* result, void\* params) = 0;  
Calls the CAPL function with the specified parameters.  
Params is a pointer to the parameter list.  
Result contains the return value of the CAPL function as double.

| ![Note](../../../../Resources/vImages/vInfo.png "Note") | Note<br>The dimensions of array parameters cannot be requested. Overloaded callback functions must therefore differ in at least one parameter type. |
| ------------------------------------------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------- |

| ![Note](../../../../Resources/vImages/vInfo.png "Note") | Note<br>It is not permitted to<br>call callback functions on additional threads used in the CAPL DLL. CAPL has no way of securing access to variables shared between threads.call CAPL functions from additional threads.<br>Otherwise, sporadic errors such as incorrect data or even program crashes may occur.<br>To fetch data from an additional thread used in the DLL, you must use a timer in CAPL, in whose [on timer](../../../CAPLFunctions/Other/EventProcedures/CAPLfunctionOnTimer.htm) routine it is checked whether data is available and copied if necessary. This operation must be secured in the DLL using the usual means of C/C++ (e.g. a std::mutex). |
| ------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |

## [ClosedVersion Control](javascript:void(0))

| ![Example](../../../../Resources/vImages/vExample.png "Example") | Example: Definition of a Version Number<br>The DLL can export a version number, which is then evaluated in the [#pragma library command](../General/CompilerCommandsPragmaLibrary.htm). The version number must be an exported variable of type unsigned long with the name caplDllLibraryVersion.<br>#define LIBRARY\_VERSION 3   CAPLEXPORT unsigned long far caplDllLibraryVersion = LIBRARY\_VERSION; |
| ---------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |

| ![Example](../../../../Resources/vImages/vExample.png "Example") | Example: Check the Version Number<br>If the version number in the [#pragma library command](../General/CompilerCommandsPragmaLibrary.htm) does not correspond with the exported version, the CAPL compiler issues a warning. The DLL can specify the warning text to be issued by exporting is as a text variable with the name caplDllVersionCheckMessage. If such a text variable is exported the CAPL compiler outputs the predefined text.<br>CAPLEXPORT const char\* far caplDllVersionCheckMessage = "Please write CAPL code for use of DLL version 3"; |
| ---------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |

| ![Example](../../../../Resources/vImages/vExample.png "Example") | Example: Error Message in case of missing Version Number<br>With an exported flag, the DLL can also instruct the compiler to issue an error when the DLL is used without a #pragma library directive and a version number in the program. This flag has to be a variable with the name caplDllRequireVersionCheck.<br>CAPLEXPORT unsigned long far caplDllRequireVersionCheck = 1; |
| ---------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |

 

- [../../../Shared/HowToUseOnlineHelp.htm](../../../Shared/HowToUseOnlineHelp.htm "Tips for using the help") [](<> "Opens German help") [](<> "Opens English help") [](<> "Opens Japanese help")[../../../Shared/HowToUseOnlineHelp.htm#BMLanguage](../../../Shared/HowToUseOnlineHelp.htm#BMLanguage) [Vector Help Dashboard](https://help.vector.com/ "Opens the Vector Help Dashboard") â¢ [Version Selection](https://help.vector.com/CANoeDEFamily/index.html "Opens the version overview of your product") 2025-10-11T20:31:10

- Â© Vector Informatik GmbH CANoe (Desktop Editions & Test Bench Editions) Version 19.3.1 [Contact/Copyright/License](../../../Shared/ContactCopyrightLicense.htm) Cookie Settings [Data Privacy Notice](https://www.vector.com/int/en/company/get-info/privacy-policy/)