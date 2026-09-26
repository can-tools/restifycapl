---
meta-msapplication-config: ../../../../../Skins/Favicons/browserconfig.xml
meta-viewport: width=device-width, initial-scale=1.0
title: Example of a Windows DLL for CAPL
---

[Open topic with navigation](../../../../../CANoe.htm#Topics/ProgrammingInterfaces/CAPL/CAPLDLL/CAPLIncludeWindowsDLLExample.htm)

[CAPL Introduction](../CAPLIntroduction.htm) Â» [CAPL DLL](CAPLIncludeWindowsDLL.htm) Â» Example of a Windows DLL for CAPL

# Example of a Windows DLL for CAPL

 

[Valid for](../../../Shared/FeatureAvailability.htm "Feature availabilty for your product"): CANoe DE â¢ CANoe MedTech DE â¢ CANoe4SW:lite DE

The following folder [Open Folder](javascript:startCANoeLauncher('"SAMPLES:\\Programming\\CAPLdll"')) contains sample projects with the sources of a CAPL DLL for Microsoft Visual Studio projects.

Copy the generated DLL into the Exec32 directory of the installation (see [Search order for a Windows DLL](CAPLIncludeWindowsDLLSearchSequence.htm)).

| ![Note](../../../../Resources/vImages/vInfo.png "Note") | Note for CANoe DE,<br>A 64-bit DLL is required for the Measurement Setup.<br>The sample project also contains the code for this case. |
| ------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------- |

The DLL can be integrated in your CANoe DE product as following:

* Add the CAPL DLL into the [Options](../../../CANoeCANalyzer/Ribbon/File/Options/Programming/ProgrammingCAPLDLL.htm) dialog.
* The DLL can be included directly in CAPL using #pragma library("File path") command.

The subdirectory \Example contains a configuration.

You can use the keyboard to call various functions of the CAPL DLL.

#define USECDLL_FEATURE  
#define _BUILDNODELAYERDLL  
#pragma warning( disable : 4786 )  
#include "..\Includes\cdll.h"  
#include "..\Includes\via.h"  
#include "..\Includes\via_CDLL.h"  
#include <stdio.h>  
#include <stdlib.h>  
#include <map>  
class CaplInstanceData;  
typedef std::map<uint32, CaplInstanceData\*> VCaplMap;

// ============================================================================  

// global variables  

// ============================================================================  
static unsigned long data = 0;  
static char dlldata[100];  
char gModuleName[_MAX_FNAME]; // filename of this DLL  
HINSTANCE gModuleHandle; // windows instance handle of this DLL  
VCaplMap gCaplMap;

// ============================================================================  
// CaplInstanceData  
//  
// Data local for a single CAPL Block.  
//  
// A CAPL-DLL can be used by more than one CAPL-Block, so every piece of  
// information thats like a globale variable in CAPL, must now be wraped into  
// an instance of an object.  
// ============================================================================  
class CaplInstanceData  
{  
public:  
CaplInstanceData(VIACapl\* capl);  
  
void GetCallbackFunctions();  
void ReleaseCallbackFunctions();  
 // Definition of the class function.  
 // This class function will call the CAPL callback functions  
uint32 ShowValue(uint32 x);  
uint32 ShowDates(int16 x, uint32 y, int16 z);  
void DllInfo(char\* x);  
void ArrayValues(uint32 flags, uint32 numberOfDatabytes, uint8 databytes[], uint8 controlcode);  
void DllVersion(char\* y);  
private:  
 // Pointer of the CAPL callback functions  
VIACaplFunction\* mShowValue;  
VIACaplFunction\* mShowDates;  
VIACaplFunction\* mDllInfo;  
VIACaplFunction\* mArrayValues;  
VIACaplFunction\* mDllVersion;  
VIACapl\* mCapl;  
};

CaplInstanceData::CaplInstanceData(VIACapl\* capl)  
 // This function will initialize the CAPL callback function with the NLL Pointer  
: mCapl(capl),  
mShowValue(NULL),  
mShowDates(NULL),  
mDllInfo(NULL),  
mArrayValues(NULL),  
mDllVersion(NULL)  
{}

static bool sCheckParams(VIACaplFunction\* f, char rtype, char\* ptype)  
{  
char type;  
int32 pcount;  
VIAResult rc;  
 // check return type  
rc = f->ResultType(&type);  
if (rc!=kVIA_OK || type!=rtype)  
{  
return false;  
}  
 // check number of parameters  
rc = f->ParamCount(&pcount);  
if (rc!=kVIA_OK || strlen(ptype)!=pcount )  
{  
return false;  
}  
 // check type of parameters  
for (int i=0; i<pcount; ++i)  
{  
rc = f->ParamType(&type, i);  
if (rc!=kVIA_OK || type!=ptype[i])  
{  
return false;  
}  
}  
return true;  
}

static VIACaplFunction\* sGetCaplFunc(VIACapl\* capl, const char \* fname, char rtype, char\* ptype)  
{  
VIACaplFunction\* f;  
 // get capl function object  
VIAResult rc = capl->GetCaplFunction(&f, fname);  
if (rc!=kVIA_OK || f==NULL)  
{  
return NULL;  
}  
 // check signature of function  
if ( sCheckParams(f, rtype, ptype) )  
{  
return f;  
}  
else  
{  
capl->ReleaseCaplFunction(f);  
return NULL;  
}  
}

void CaplInstanceData::GetCallbackFunctions()  
{  
 // Get a CAPL function handle. The handle stays valid until  

// end of measurement or a call of ReleaseCaplFunction.  
mShowValue = sGetCaplFunc(mCapl, "CALLBACK_ShowValue", 'D', "D");  
mShowDates = sGetCaplFunc(mCapl, "CALLBACK_ShowDates", 'D', "IDI");  
mDllInfo = sGetCaplFunc(mCapl, "CALLBACK_DllInfo", 'V', "C");  
mArrayValues = sGetCaplFunc(mCapl, "CALLBACK_ArrayValues", 'V', "DBB");  
mDllVersion = sGetCaplFunc(mCapl, "CALLBACK_DllVersion", 'V', "C");  
}

void CaplInstanceData::ReleaseCallbackFunctions()  
{  
 // Release all the requested Callback functions  
mCapl->ReleaseCaplFunction(mShowValue);  
mShowValue = NULL;  
mCapl->ReleaseCaplFunction(mShowDates);  
mShowDates = NULL;  
mCapl->ReleaseCaplFunction(mDllInfo);  
mDllInfo = NULL;  
mCapl->ReleaseCaplFunction(mArrayValues);  
mArrayValues = NULL;  
mCapl->ReleaseCaplFunction(mDllVersion);  
mDllVersion = NULL;  
}

void CaplInstanceData::DllVersion(char\* y)  
{  
 // Prepare the parameters for the call stack of CAPL.  
// Arrays uses a 8 byte on the stack, 4 Bytes for the number of element,  
// and 4 bytes for the pointer to the array  
int32 sizeX = strlen(y)+1;  
uint8 params[8]; // parameters for call stack, 8 Bytes total  
memcpy(params+0, &sizeX, 4); // array size of first parameter, 4 Bytes  
memcpy(params+4, &y, 4); // array pointer of first parameter, 4 Bytes  
  
if(mDllVersion!=NULL)  
{  
uint32 result; // dummy variable  
VIAResult rc = mDllVersion->Call(&result, params);  
}  
}

uint32 CaplInstanceData::ShowValue(uint32 x)  
{  
void\* params = &x; // parameters for call stack  
uint32 result;  
if(mShowValue!=NULL)  
{  
VIAResult rc = mShowValue->Call(&result, params);  
if (rc==kVIA_OK)  
{  
return result;  
}  
}  
return -1;  
}

uint32 CaplInstanceData::ShowDates(int16 x, uint32 y, int16 z)  
{  
 // Prepare the parameters for the call stack of CAPL. The stack grows  
// from top to down, so the first parameter in the parameter list is the last  
// one in memory. CAPL uses also a 32 bit alignment for the parameters.  
uint8 params[12]; // parameters for call stack, 12 Bytes total  
memcpy(params+0, &z, 2); // third parameter, offset 0, 2 Bytes  
memcpy(params+4, &y, 4); // second parameter, offset 4, 4 Bytes  
memcpy(params+8, &x, 2); // first parameter, offset 8, 2 Bytes  
uint32 result;  
if(mShowDates!=NULL)  
{  
VIAResult rc = mShowDates->Call(&result, params);  
if (rc==kVIA_OK)  
{  
return rc; // call successful  
}  
}  
  
return -1; // call failed  
}

void CaplInstanceData::DllInfo(char\* x)  
{  
 // Prepare the parameters for the call stack of CAPL.  
// Arrays uses a 8 byte on the stack, 4 Bytes for the number of element,  
// and 4 bytes for the pointer to the array  
int32 sizeX = strlen(x)+1;  
uint8 params[8]; // parameters for call stack, 8 Bytes total  
memcpy(params+0, &sizeX, 4); // array size of first parameter, 4 Bytes  
memcpy(params+4, &x, 4); // array pointer of first parameter, 4 Bytes  
  
if(mDllInfo!=NULL)  
{  
uint32 result; // dummy variable  
VIAResult rc = mDllInfo->Call(&result, params);  
}  
}

void CaplInstanceData::ArrayValues(uint32 flags, uint32 numberOfDatabytes, uint8 databytes[], uint8 controlcode)  
{  
 // Prepare the parameters for the call stack of CAPL. The stack grows  
// from top to down, so the first parameter in the parameter list is the last  
// one in memory. CAPL uses also a 32 bit alignment for the parameters.  
// Arrays uses a 8 byte on the stack, 4 Bytes for the number of element,  
// and 4 bytes for the pointer to the array  
  
uint8 params[16]; // parameters for call stack, 16 Bytes total  
memcpy(params+ 0, &controlcode, 1); // third parameter, offset 0, 1 Bytes  
memcpy(params+ 4, &numberOfDatabytes, 4); // second parameter (array size), offset 4, 4 Bytes  
memcpy(params+ 8, &databytes, 4); // second parameter (array pointer), offset 8, 4 Bytes  
memcpy(params+12, &flags, 4); // first parameter, offset 12, 4 Bytes  
if(mArrayValues!=NULL)  
{  
uint32 result; // dummy variable  
VIAResult rc = mArrayValues ->Call(&result, params);  
}  
}

// ============================================================================  
// CaplInstanceData  
//  
// Data local for a single CAPL Block.  
//  
// A CAPL-DLL can be used by more than one CAPL-Block, so every piece of  
// information thats like a globale variable in CAPL, must now be wraped into  
// an instance of an object.  
// ============================================================================  

void CAPLEXPORT far CAPLPASCAL appInit (uint32 handle)  
{  
CaplInstanceData\* inst = gCaplMap[handle];  
if (inst==NULL)  
{  
return;  
}  
inst->GetCallbackFunctions();  
}

void CAPLEXPORT far CAPLPASCAL appEnd (uint32 handle)  
{  
CaplInstanceData\* inst = gCaplMap[handle];  
if (inst==NULL)  
{  
return;  
}  
inst->ReleaseCallbackFunctions();  
gCaplMap.erase(handle);  
}

long CAPLEXPORT far CAPLPASCAL appSetValue (uint32 handle, long x)  
{  
CaplInstanceData\* inst = gCaplMap[handle];  
if (inst==NULL)  
{  
return -1;  
}  
return inst->ShowValue(x);;  
}

long CAPLEXPORT far CAPLPASCAL appReadData (uint32 handle, long a)  
{  
CaplInstanceData\* inst = gCaplMap[handle];  
if (inst==NULL)  
{  
return -1;  
}  
int16 x = (a>=0) ? +1 : -1;  
uint32 y = abs(a);  
int16 z = (int16)(a & 0x0f000000) >> 24;  
inst->DllVersion("Version 1.1");  
  
inst->DllInfo("DLL: processing");  
uint8 databytes[8] = { 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88};  
inst->ArrayValues( 0xaabbccdd, sizeof(databytes), databytes, 0x01);  
return inst->ShowDates( x, y, z);  
}

// ============================================================================  
// VIARegisterCDLL  
// ============================================================================  

VIACLIENT(void) VIARegisterCDLL (VIACapl\* service)  
{  
uint32 handle;  
VIAResult result;  
if (service==NULL)  
{  
return;  
}  
result = service->GetCaplHandle(&handle);  
if(result!=kVIA_OK)  
{  
return;  
}  
gCaplMap[handle] = new CaplInstanceData(service);  
}

void CAPLEXPORT far CAPLPASCAL voidFct( void )  
{  
 // do something  
data = 55;  
}

unsigned long CAPLEXPORT far CAPLPASCAL appLongFuncName( void )  
{  
return 1;  
}

void CAPLEXPORT far CAPLPASCAL appPut(unsigned long x)  
{  
data = x;  
}

unsigned long CAPLEXPORT far CAPLPASCAL appGet(void)  
{  
return data;  
}

long CAPLEXPORT far CAPLPASCAL appAdd(long x, long y)  
{  
long z = x + y;  
return z;  
}

long CAPLEXPORT far CAPLPASCAL appSubtract(long x, long y)  
{  
long z = x - y;  
return z;  
}

void CAPLEXPORT far CAPLPASCAL appGetDataTwoPars( unsigned long numberBytes,  
unsigned char dataBlock[] )  
{  
unsigned int i;  
for (i = 0; i < numberBytes; i++) {  
dataBlock[i]= dlldata[i];  
}  
}

void CAPLEXPORT far CAPLPASCAL appPutDataTwoPars( unsigned long numberBytes,  
const unsigned char dataBlock[] )  
{  
unsigned int i;  
for (i = 0; i < numberBytes; i++) {  
dlldata[i] = dataBlock[i];  
}  
}

// get data from DLL into CAPL memory  

void CAPLEXPORT far CAPLPASCAL appGetDataOnePar( unsigned char dataBlock[] )  
{  
 // get first element  
dataBlock[0] = (unsigned char)data;  
}

// put data from CAPL array to DLL  

void CAPLEXPORT far CAPLPASCAL appPutDataOnePar( const unsigned char dataBlock[] )  
{  
 // put first element  
data = dataBlock[0];  
}

// ============================================================================  
// CAPL_DLL_INFO_LIST : list of exported functions  
// The first field is predefined and mustn't be changed!  
// The list has to end with a {0,0} entry!  
// New struct supporting function names with up to 50 characters  
// ============================================================================  

CAPL_DLL_INFO3 table[] = {  
{CDLL_VERSION_NAME, (CAPL_FARCALL)CDLL_VERSION, "", "", CAPL_DLL_CDECL, 0xabcd, CDLL_EXPORT },  
{"dllInit", (CAPL_FARCALL)appInit, "CAPL_DLL","This function will initialize all callback functions in the CAPLDLL",'V', 1, "D", "", {"handle"}},  
{"dllEnd", (CAPL_FARCALL)appEnd, "CAPL_DLL","This function will release the CAPL function handle in the CAPLDLL",'V', 1, "D", "", {"handle"}},  
{"dllSetValue", (CAPL_FARCALL)appSetValue, "CAPL_DLL","This function will call a callback functions",'L', 2, "DL", "", {"handle","x"}},  
{"dllReadData", (CAPL_FARCALL)appReadData, "CAPL_DLL","This function will call a callback functions",'L', 2, "DL", "", {"handle","x"}},  
{"dllPut", (CAPL_FARCALL)appPut, "CAPL_DLL","This function will save data from CAPL to DLL memory",'V', 1, "D", "", {"x"}},  
{"dllGet", (CAPL_FARCALL)appGet, "CAPL_DLL","This function will read data from DLL memory to CAPL",'D', 0, "", "", {""}},  
{"dllVoid", (CAPL_FARCALL)voidFct, "CAPL_DLL","This function will overwrite DLL memory from CAPL without parameter",'V', 0, "", "", {""}},  
{"dllPutDataOnePar", (CAPL_FARCALL)appPutDataOnePar, "CAPL_DLL","This function will put data from CAPL array to DLL",'V', 1, "B", "\001", {"datablock"}},  
{"dllGetDataOnePar", (CAPL_FARCALL)appGetDataOnePar, "CAPL_DLL","This function will get data from DLL into CAPL memory",'V', 1, "B", "\001", {"datablock"}},  
{"dllPutDataTwoPars", (CAPL_FARCALL)appPutDataTwoPars,"CAPL_DLL","This function will put two datas from CAPL array to DLL",'V', 2, "DB", "\000\001", {"noOfBytes","datablock"}}, // number of pars in octal format  
{"dllGetDataTwoPars", (CAPL_FARCALL)appGetDataTwoPars,"CAPL_DLL","This function will get two datas from DLL into CAPL memory",'V', 2, "DB", "\000\001", {"noOfBytes","datablock"}},  
{"dllAdd", (CAPL_FARCALL)appAdd, "CAPL_DLL","This function will add two values. The return value is the result",'L', 2, "LL", "", {"x","y"}},  
{"dllSubtract", (CAPL_FARCALL)appSubtract, "CAPL_DLL","This function will substract two values. The return value is the result",'L', 2, "LL", "", {"x","y"}},  
{"dllSupportLongFunctionNamesWithUpTo50Characters", (CAPL_FARCALL)appLongFuncName, "CAPL_DLL","This function shows the support of long function names",'D', 0, "", "", {""}},  
{0, 0}  
};

CAPLEXPORT CAPL_DLL_INFO3 far \* caplDllTable3 = table;  
// ============================================================================  
// DllMain, entry Point of DLL  
// ============================================================================  

BOOL WINAPI DllMain(HINSTANCE handle, dword reason, void\*)  
{  
switch (reason)  
{  
case DLL_PROCESS_ATTACH:  
{  
gModuleHandle = handle;  
  
// Get full filename of module  
char path_buffer[_MAX_PATH];  
dword result = GetModuleFileName(gModuleHandle, path_buffer,_MAX_PATH);  
// split filename into parts  
char drive[_MAX_DRIVE];  
char dir[_MAX_DIR];  
char fname[_MAX_FNAME];  
char ext[_MAX_EXT];  
_splitpath_s( path_buffer, drive, dir, fname, ext );  
  
strcpy_s(gModuleName, fname);  
  
return 1; // Indicate that the DLL was initialized successfully.  
}  
case DLL_PROCESS_DETACH:  
{  
return 1; // Indicate that the DLL was detached successfully.  
}  
}  
return 1;  
}

 

- [../../../Shared/HowToUseOnlineHelp.htm](../../../Shared/HowToUseOnlineHelp.htm "Tips for using the help") [](<> "Opens German help") [](<> "Opens English help") [](<> "Opens Japanese help")[../../../Shared/HowToUseOnlineHelp.htm#BMLanguage](../../../Shared/HowToUseOnlineHelp.htm#BMLanguage) [Vector Help Dashboard](https://help.vector.com/ "Opens the Vector Help Dashboard") â¢ [Version Selection](https://help.vector.com/CANoeDEFamily/index.html "Opens the version overview of your product") 2025-10-11T20:31:10

- Â© Vector Informatik GmbH CANoe (Desktop Editions & Test Bench Editions) Version 19.3.1 [Contact/Copyright/License](../../../Shared/ContactCopyrightLicense.htm) Cookie Settings [Data Privacy Notice](https://www.vector.com/int/en/company/get-info/privacy-policy/)