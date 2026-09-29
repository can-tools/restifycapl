---
meta-msapplication-config: ../../../../../Skins/Favicons/browserconfig.xml
meta-viewport: width=device-width, initial-scale=1.0
title: Declaration and Initialization of Variables
---

[Open topic with navigation](../../../../../CANoe.htm#Topics/ProgrammingInterfaces/CAPL/General/VariablesDeclarationInitialization.htm)

[CAPL Introduction](../CAPLIntroduction.htm) Â» Declaration and Initialization of Variables

# Declaration and Initialization of Variables

 

[Valid for](../../../Shared/FeatureAvailability.htm "Feature availabilty for your product"): CANoe DE â¢ CANoe MedTech DE â¢ CANoe4SW:lite DE

## Global Variables

In the Browser, global variables are declared in the Variables section. The [data types](DataTypesForVariables.htm) DWORD, LONG, WORD, INT, BYTE and CHAR can be used analogously to their use in the C programming language. The data types QWORD and INT64 are used for 64 bit integers. The data types FLOAT and DOUBLE are synonyms and designate 64 bit floating point numbers conforming to the IEEE standard.

A timer is created with timer. The timer does not begin to run until it has been started. After the timer has elapsed the associated [on timer](../../../CAPLFunctions/Other/EventProcedures/CAPLfunctionOnTimer.htm) [event procedure](../../../CAPLFunctions/Other/CAPLfunctionsEventProceduresOverview.htm) is called. A variable of the type timer can only be accessed by the predefined functions [setTimer](../../../CAPLFunctions/Other/Functions/CAPLfunctionSetTimer.htm) and [cancelTimer](../../../CAPLFunctions/Other/Functions/CAPLfunctionCancelTimer.htm).

CAN messages to be output by the CAPL program are declared with message. Components of the objects are accessed using [selectors](../../../CAPLFunctions/CAN/CAPLfunctionMessageSelectors.htm).

Variables can be initialized in their declarations. Both simplified notation and bracketing with { } are permitted. The compiler initializes all variables, with the exception of timers, with default values (automatic default: 0). The message field DIR is usually initialized with TXREQUEST. However, variables of the type message can also be initialized explicitly when they are declared. In initializing messages the data area can be accessed by indicating the type and byte offset. Symbolic identifiers are entered to initialize control areas.

[Declaration of arrays](DeclarationOfArrays.htm) (arrays, vectors, matrices) is permitted in CAPL, including for variables of the type message.

## Local Variables

Local variables are created statically in CAPL (in contrast to C) by default. This means that an initialization is only executed at the program start, and when variables enter the procedure they assume the value they had when they last left the procedure.

In order to create a local variable on the stack, you can use the key word stack before the declaration as of version 12.0 SP3. A variable created on the stack is re-initialized after each declaration run-through. If the function in which the variable has been declared is carried out several times in parallel, e.g. by calling [testStartParallel](../../../CAPLFunctions/Test/Functions/CAPLfunctionTestStartParallel.htm), each execution has its own value for the variable while a static variable is used jointly by all executions. The key word stack always refers to all variables within a declaration (i.e. up to the semicolon).

For statically generated variables, only constant expressions are allowed for the initialization. As of version 13.0, you can also initialize variables generated on the stack using expressions that are evaluated during runtime.

| ![Example](../../../../Resources/vImages/vExample.png "Example") | Example<br>Simple types:<br>int j, k = 2; // j = 0   double f = 17.5;   stack long l1 = 1, l2 = 2; // set to 1 and 2 on each invocation of the method   msTimer t1; // No initialization<br>Initialization of message variables:<br>message 100 msg = {dlc = 4, word(0) = 0x1234};<br>Arrays:<br>int lookUpTable[3] = {1,2,3};   char text[12] = "Hello world";   int matrix[2][2] = {{11,12},{21,22}}; |
| ---------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |

[Declaration of Messages](DeclarationOfMessages.htm) â¢ [Declaration of Arrays](DeclarationOfArrays.htm)

- [../../../Shared/HowToUseOnlineHelp.htm](../../../Shared/HowToUseOnlineHelp.htm "Tips for using the help") [](<> "Opens German help") [](<> "Opens English help") [](<> "Opens Japanese help")[../../../Shared/HowToUseOnlineHelp.htm#BMLanguage](../../../Shared/HowToUseOnlineHelp.htm#BMLanguage) [Vector Help Dashboard](https://help.vector.com/ "Opens the Vector Help Dashboard") â¢ [Version Selection](https://help.vector.com/CANoeDEFamily/index.html "Opens the version overview of your product") 2025-10-11T20:31:10

- Â© Vector Informatik GmbH CANoe (Desktop Editions & Test Bench Editions) Version 19.3.1 [Contact/Copyright/License](../../../Shared/ContactCopyrightLicense.htm) Cookie Settings [Data Privacy Notice](https://www.vector.com/int/en/company/get-info/privacy-policy/)