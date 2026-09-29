---
meta-msapplication-config: ../../../../../Skins/Favicons/browserconfig.xml
meta-viewport: width=device-width, initial-scale=1.0
title: Declaration of Messages
---

[Open topic with navigation](../../../../../CANoe.htm#Topics/ProgrammingInterfaces/CAPL/General/DeclarationOfMessages.htm)

[CAPL Introduction](../CAPLIntroduction.htm) Â» Declaration of Messages

# Declaration of Messages

 

[Valid for](../../../Shared/FeatureAvailability.htm "Feature availabilty for your product"): CANoe DE

Messages to be output by the CAPL program are declared with the [keyword](Keywords.htm) message. The complete declaration includes the message identifier or message name when working with symbolic databases. For example, to output on the bus the messages with identifiers A (hex) and 100 (dec) and the message EngineData defined in the [database](SymbolicAccessSignal.htm).

You would write:

message 0xA m1;  
message 100 m2;  
message EngineData m3;  
...  
output(m1);  
output(m2);  
output(m3);

The message identifier is entered as a number (integer) in decimal or hexadecimal notation to identify messages. An x is appended to [extended identifiers](../../../CANoeCANalyzer/General/CANExtendedIdentifier.htm). The entry of \* signifies that at first this variable does not have any message identifier. The identifier must then be established in another manner before the object is sent out. For example, such objects can serve in filtering tasks to save all objects which are to be passed unchanged. (In this case, the received message is copied, including its message identifier.)

The data area of a message is accessed by entering the data type as a data selector and the byte-value offset (starting with 0). The following data selectors can be used: LONG, DWORD, INT, WORD, CHAR and BYTE.

For example, the following would result in transmission of a message 100 with DLC = 1 and first data byte 0xFF on the bus:

message 100 msg;  
msg.DLC = 1;  
msg.BYTE(0) = 0xff;  
output(msg);

Components of objects are accessed by [selectors](../../../CAPLFunctions/CAN/CAPLfunctionMessageSelectors.htm). If you want the object to be defined for a specific chip (for CAN cards with more than one CAN chip), you would insert the prefix of the particular selector (CAN1 or CAN2) followed by a decimal point (.) before the message.

[Declaration and Initialization of Global Variables](VariablesDeclarationInitialization.htm) â¢ [Declaration of Arrays](DeclarationOfArrays.htm) â¢ [Symbolic Access to Message Attributes](SymbolicAccessMessageAttributes.htm) â¢ [Resolution of Ambiguities](ResolveAmbiguities.htm)

- [../../../Shared/HowToUseOnlineHelp.htm](../../../Shared/HowToUseOnlineHelp.htm "Tips for using the help") [](<> "Opens German help") [](<> "Opens English help") [](<> "Opens Japanese help")[../../../Shared/HowToUseOnlineHelp.htm#BMLanguage](../../../Shared/HowToUseOnlineHelp.htm#BMLanguage) [Vector Help Dashboard](https://help.vector.com/ "Opens the Vector Help Dashboard") â¢ [Version Selection](https://help.vector.com/CANoeDEFamily/index.html "Opens the version overview of your product") 2025-10-11T20:31:10

- Â© Vector Informatik GmbH CANoe (Desktop Editions & Test Bench Editions) Version 19.3.1 [Contact/Copyright/License](../../../Shared/ContactCopyrightLicense.htm) Cookie Settings [Data Privacy Notice](https://www.vector.com/int/en/company/get-info/privacy-policy/)