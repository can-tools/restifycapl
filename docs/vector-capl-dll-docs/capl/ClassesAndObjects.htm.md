---
meta-msapplication-config: ../../../../../Skins/Favicons/browserconfig.xml
meta-viewport: width=device-width, initial-scale=1.0
title: Classes and Objects in CAPL
---

[Open topic with navigation](../../../../../CANoe.htm#Topics/ProgrammingInterfaces/CAPL/General/ClassesAndObjects.htm)

[CAPL Introduction](../CAPLIntroduction.htm) Â» Classes and Objects in CAPL

# Classes and Objects in CAPL

 

[Valid for](../../../Shared/FeatureAvailability.htm "Feature availabilty for your product"): CANoe DE â¢ CANoe MedTech DE â¢ CANoe4SW:lite DE

In CAPL predefined data types are available that can be used like classes in object-oriented programming languages such as C++.

This means that functions in the form of methods can be called on variables (objects) of these data types (classes), and that under certain circumstances destructors can be called automatically to stop a process and invalidate the variable.

## Classes (predefined data types)

* [Associative fields](../../../CAPLFunctions/ObjectOrientedProg/CAPLfunctionsOOPAssociativeFields.htm)
* [CAN](../../../CAPLFunctions/ObjectOrientedProg/CAPLfunctionsOOPCAN.htm)
* [CAPLProfiler](../../../CAPLFunctions/ObjectOrientedProg/CAPLfunctionsOOPCAPLProfiler.htm)
* [Classes of CAN Disturbance Interface](../../../CAPLFunctions/CANDisturbance/CAPLfunctionsClassesOverview.htm)
* [Classes of Scope](../../../CAPLFunctions/Scope/CAPLfunctionsScopeOverview.htm)
* [DiagRequest](../../../CAPLFunctions/ObjectOrientedProg/CAPLfunctionsOOPDiagRequest.htm)
* [DiagResponse](../../../CAPLFunctions/ObjectOrientedProg/CAPLfunctionsOOPDiagResponse.htm)
* [File](../../../CAPLFunctions/ObjectOrientedProg/CAPLfunctionsOOPFile.htm)
* [TestCheck](../../../CAPLFunctions/ObjectOrientedProg/CAPLfunctionsOOPTestCheck.htm)
* [TestStimulus](../../../CAPLFunctions/ObjectOrientedProg/CAPLfunctionsOOPTestStimulus.htm)
* [Timer, MsTimer](../../../CAPLFunctions/ObjectOrientedProg/CAPLfunctionsOOPTimer.htm)
* [TcpSocket](../../../CAPLFunctions/ObjectOrientedProg/CAPLfunctionsOOPTCPSocket.htm)
* [UdpSocket](../../../CAPLFunctions/ObjectOrientedProg/CAPLfunctionsOOPUDPSocket.htm)

## Method Call

Methods are called on a variable of a class type by writing a point and the method name after the variable name.

| ![Example](../../../../Resources/vImages/vExample.png "Example") | Example<br>Method call<br>myTimer.set(500);<br>Function call<br>setTimer(myTimer, 500); |
| ---------------------------------------------------------------- | --------------------------------------------------------------------------------------- |

## Automatic Destructors

If you declare the variable of a class type within a function or a test case, but not in the global variables section, a destructor will be called on this variable as soon as the process leaves the block in which the variable has been declared.

| ![Example](../../../../Resources/vImages/vExample.png "Example") | Example<br>if (someCondition)   {    File file1("C:\\test.TXT",1,0);    // ...   } // file1 is closed here automatically |
| ---------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------ |

If you declare the variable of a class type in the global variables section, the destructor will not be called automatically. You will need to call it explicitly once the object is no longer needed. The call corresponds to a call to a standard method.

A renewed call of a destructor on a already destructed object has no effect.

## Constructors

Most variables of class types which have destructors have to be initialized explicitly before they can be used. The initialization functions used for this purpose are called constructors.

There are two types of constructors:

* Simple constructors  
Simple constructors can transfer the necessary parameters directly when a variable is declared:

    file f("C:\\test.TXT", 0, 0);

    However, this is not permitted if the variable has been declared in the global variables section. In this case, the constructor has to be called explicitly:

    file f;  
f.open("C:\\test.TXT", 0, 0);

* Constructor functions  
Constructor functions are also used for initialization. Their call is class name::method name:

    TestCheck c;  
c = TestCheck::CreateTimeout(500);

If a constructor is called again on an object which has already been initialized, the destructor of the old object is called first and then the variable is initialized.

In the case of constructor functions, the function is evaluated before the old object is destroyed.

| ![Example](../../../../Resources/vImages/vExample.png "Example") | Example<br>TestCheck c;   c = TestCheck::CreateTimeout(500);   // Sequence:    // 1. Call CreateTimeout, i.e. build new check    // 2. destruct old check    // 3. initialize c with the new check   c = TestCheck::CreateTimeout(200); |
| ---------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |

| ![Note](../../../../Resources/vImages/vInfo.png "Note") | Note<br>Variables of the same class type cannot be assigned to one another. However, they can be transferred to functions as parameters. Please bear in mind that in this case multiple variables might stand for the same object and that calling a destructor on one variable will invalidate the other variables.You can also generate fields from variables of the same class type, but only in the global variables section. Structures or associative fields cannot contain variables of a class type. |
| ------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |

[Data Types for Variables](DataTypesForVariables.htm)

- [../../../Shared/HowToUseOnlineHelp.htm](../../../Shared/HowToUseOnlineHelp.htm "Tips for using the help") [](<> "Opens German help") [](<> "Opens English help") [](<> "Opens Japanese help")[../../../Shared/HowToUseOnlineHelp.htm#BMLanguage](../../../Shared/HowToUseOnlineHelp.htm#BMLanguage) [Vector Help Dashboard](https://help.vector.com/ "Opens the Vector Help Dashboard") â¢ [Version Selection](https://help.vector.com/CANoeDEFamily/index.html "Opens the version overview of your product") 2025-10-11T20:31:10

- Â© Vector Informatik GmbH CANoe (Desktop Editions & Test Bench Editions) Version 19.3.1 [Contact/Copyright/License](../../../Shared/ContactCopyrightLicense.htm) Cookie Settings [Data Privacy Notice](https://www.vector.com/int/en/company/get-info/privacy-policy/)