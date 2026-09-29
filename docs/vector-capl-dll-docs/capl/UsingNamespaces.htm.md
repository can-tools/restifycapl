---
meta-msapplication-config: ../../../../../Skins/Favicons/browserconfig.xml
meta-viewport: width=device-width, initial-scale=1.0
title: Using Namespaces
---

[Open topic with navigation](../../../../../CANoe.htm#Topics/ProgrammingInterfaces/CAPL/General/UsingNamespaces.htm)

[CAPL-EinfÃ¼hrung](../CAPLIntroduction.htm) Â» Using Namespaces

# Using Namespaces

 

[Valid for](../../../Shared/FeatureAvailability.htm "Feature availabilty for your product"): CANoe DE

You can use the using namespace directive to abbreviate long namespaces for [system variables](../../../SystemVariables/SysVar.htm) or [distributed objects](../../../CANoeCANalyzer/CommunicationConcept/CCDistributedObjects.htm). You can use the using namespace <Namespace Name> directive in the variables section:

{  
using namespace A::B;  
}

For a valid declaration, the namespace A with a namespace B contained in it must be present either in the system variables or in the distributed objects.

If, for example, a system variable A::B::x is available, then this can also be used directly as x. Using A::B::x is also still possible.

If the use of a system variable is no longer unique due to a using namespace declaration, this results in a compilation error. This can occur if variables with the same name exist in several namespaces. If the problem occurs, you can solve it by completely qualifying the names of the system variables.

It is not possible to assign a new name to an existing namespace.

| ![Example](../../../../Resources/vImages/vExample.png "Example") | Example  | [vCDL](../../vCDL/vCDLStartpage.htm)                                                                                                                                                                     | CAPL                                                                                                                                                                                                     | | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | | version 2.0;    namespace SomeNamespace   {    interface SomeInterface    {    internal data int32 X;    }    namespace InnerNamespace    {    SomeInterface SomeObject {}    SomeInterface AmbiguousObject {}    }    SomeInterface AmbiguousObject {}   } | variables   {    using namespace SomeNamespace;    using namespace SomeNamespace::InnerNamespace;   }    on key 'a'   {    // Ok, fully qualified name    $SomeNamespace::InnerNamespace::SomeObject.X = 42;    // Ok, uses the first using namespace directive    $InnerNamespace::SomeObject.X = 42;    // Ok, uses the second using namespace directive    $SomeObject.X = 42;     // Does not compile because the usage is not unique    $AmbiguousObject.X = 42;    // Ok, the value can always be used fully qualified    $SomeNamespace::InnerNamespace::AmbiguousObject.X = 42;   } | |
| ---------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |

 

- [../../../Shared/HowToUseOnlineHelp.htm](../../../Shared/HowToUseOnlineHelp.htm "Tips for using the help") [](<> "Opens German help") [](<> "Opens English help") [](<> "Opens Japanese help")[../../../Shared/HowToUseOnlineHelp.htm#BMLanguage](../../../Shared/HowToUseOnlineHelp.htm#BMLanguage) [Vector Help Dashboard](https://help.vector.com/ "Opens the Vector Help Dashboard") â¢ [Version Selection](https://help.vector.com/CANoeDEFamily/index.html "Opens the version overview of your product") 2025-10-11T20:31:10

- Â© Vector Informatik GmbH CANoe (Desktop Editions & Test Bench Editions) Version 19.3.1 [Contact/Copyright/License](../../../Shared/ContactCopyrightLicense.htm) Cookie Settings [Data Privacy Notice](https://www.vector.com/int/en/company/get-info/privacy-policy/)