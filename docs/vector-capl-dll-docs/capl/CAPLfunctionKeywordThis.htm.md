---
meta-msapplication-config: ../../../../../Skins/Favicons/browserconfig.xml
meta-viewport: width=device-width, initial-scale=1.0
title: Keyword this
---

[Open topic with navigation](../../../../../CANoe.htm#Topics/CAPLFunctions/Other/EventProcedures/CAPLfunctionKeywordThis.htm)

[CAPL Functions](../../CAPLfunctions.htm) Â» [General](../CAPLGeneralStartPage.htm) Â» [Event Procedures](../CAPLfunctionsEventProceduresOverview.htm) Â» Keyword this

# Keyword this

 

[Valid for](../../../Shared/FeatureAvailability.htm "Feature availabilty for your product"): CANoe DE â¢ CANoe MedTech DE â¢ CANoe4SW:lite DE

Within an [event procedure](../../../ProgrammingInterfaces/CAPL/General/EventProceduresOverview.htm) for receiving a CAN object or a variable, the data structure of the object is designated by the key word this.

For example, you could access the first data byte of message 100 which was just received by means of the following:

on message 100 {  
byte byte_0;  
byte_0 = this.byte(0);  
...  
}

Analogously, you could read the new value of the integer variable Switch which has just been changed by means of the following:

on sysVar Switch {  
int val;  
val = @this;  
...  
}

| ![Note](../../../../Resources/vImages/vInfo.png "Note") | Note<br>You should not change the value of this within an event procedure. However, to permit the use of this as a parameter, value changes made to this are not prohibited by the CAPL compiler. However, please note that these types of write accesses to this are only valid locally (i.e. within the event procedure). When compiling you will receive an appropriate warning. Thus, if you call the function [output(this)](../../CAN/Functions/CAPLfunctionOutput.htm) after this has been changed in an [on message](../../CAN/EventProcedures/CAPLfunctionOnMessage.htm) event procedure, the unchanged original of this is passed, and not your change. |
| ------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |

## Special Case: Output of signal values using this

on message 101  
{  
float a = 0;  
a = this.Signal1.phys;  
}

| ![Note](../../../../Resources/vImages/vInfo.png "Note") | Note<br>For a signal length of more than 32 bit the read out of signal values from a message with this.<signalName> is not yet supported. In this case the dollar notation must be used, see the example below. |
| ------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |

on message 101  
{  
float a = 0;  
a = $Signal1;  
}

[Access to signal values using this](../../CAN/EventProcedures/CAPLfunctionOnMessage.htm) â¢ [Selectors](../../../ProgrammingInterfaces/CAPL/General/SelectorsOverview.htm)

- [../../../Shared/HowToUseOnlineHelp.htm](../../../Shared/HowToUseOnlineHelp.htm "Tips for using the help") [](<> "Opens German help") [](<> "Opens English help") [](<> "Opens Japanese help")[../../../Shared/HowToUseOnlineHelp.htm#BMLanguage](../../../Shared/HowToUseOnlineHelp.htm#BMLanguage) [Vector Help Dashboard](https://help.vector.com/ "Opens the Vector Help Dashboard") â¢ [Version Selection](https://help.vector.com/CANoeDEFamily/index.html "Opens the version overview of your product") Technical References are only available in English 2025-10-11T20:31:10

- Â© Vector Informatik GmbH CANoe (Desktop Editions & Test Bench Editions) Version 19.3.1 [Contact/Copyright/License](../../../Shared/ContactCopyrightLicense.htm) Cookie Settings [Data Privacy Notice](https://www.vector.com/int/en/company/get-info/privacy-policy/)