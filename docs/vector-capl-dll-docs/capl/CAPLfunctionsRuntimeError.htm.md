---
meta-msapplication-config: ../../../../Skins/Favicons/browserconfig.xml
meta-viewport: width=device-width, initial-scale=1.0
title: Runtime Errors
---

[Open topic with navigation](../../../../CANoe.htm#Topics/CAPLFunctions/Other/CAPLfunctionsRuntimeError.htm)

[CAPL Functions](../CAPLfunctions.htm) Â» [General](CAPLGeneralStartPage.htm) Â» Runtime Errors

# Runtime Errors

 

[Valid for](../../Shared/FeatureAvailability.htm "Feature availabilty for your product"): CANoe DE â¢ CANoe MedTech DE â¢ CANoe4SW:lite DE

A number of runtime errors are monitored:

* Division by zero
* Exceeding upper or lower limits of the array
* Exceeding upper or lower limits of offset in the
 data range of messages.
* Stack overflow when CAPL subroutines are called.

If a runtime error is detected the function [runError](Functions/CAPLfunctionRunError.htm) is called. This outputs a comment
 to the Write Window, which contains the name of the CAPL program, the type of error and an error index. With the help of the error index, the point in the CAPL source text which generated the error is found. Measurement is terminated after output of the comment.

The user can also call the function [runError](Functions/CAPLfunctionRunError.htm) directly to generate assertions.

 

- [../../Shared/HowToUseOnlineHelp.htm](../../Shared/HowToUseOnlineHelp.htm "Tips for using the help") [](<> "Opens German help") [](<> "Opens English help") [](<> "Opens Japanese help")[../../Shared/HowToUseOnlineHelp.htm#BMLanguage](../../Shared/HowToUseOnlineHelp.htm#BMLanguage) [Vector Help Dashboard](https://help.vector.com/ "Opens the Vector Help Dashboard") â¢ [Version Selection](https://help.vector.com/CANoeDEFamily/index.html "Opens the version overview of your product") Technical References are only available in English 2025-10-11T20:31:10

- Â© Vector Informatik GmbH CANoe (Desktop Editions & Test Bench Editions) Version 19.3.1 [Contact/Copyright/License](../../Shared/ContactCopyrightLicense.htm) Cookie Settings [Data Privacy Notice](https://www.vector.com/int/en/company/get-info/privacy-policy/)