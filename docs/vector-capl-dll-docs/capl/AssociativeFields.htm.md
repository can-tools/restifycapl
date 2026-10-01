---
meta-msapplication-config: ../../../../../Skins/Favicons/browserconfig.xml
meta-viewport: width=device-width, initial-scale=1.0
title: Associative Fields
---

[Open topic with navigation](../../../../../CANoe.htm#Topics/ProgrammingInterfaces/CAPL/General/AssociativeFields.htm)

[CAPL Introduction](../CAPLIntroduction.htm) Â» Associative Fields

# Associative Fields

 

[Valid for](../../../Shared/FeatureAvailability.htm "Feature availabilty for your product"): CANoe DE â¢ CANoe MedTech DE â¢ CANoe4SW:lite DE

With associative fields (so-called maps) you can perform a 1:1 assignment of values to other values without using excessive memory. The elements of an associative field are key value pairs, whereby there is fast access to a value via a key.

An associative field is declared in a similar way to a normal field but the data type of the key is written in square brackets:

int m[float]; // maps floats to ints  
float x[int64]; // maps int64s to floats  
char[30] s[ char[] ] // maps strings (of unspecified length) to strings of length < 30

Data types for the keys can be long, int64, float, double, enumeration types and char[]. As data type for values are simple data types, enumeration types fields and structure types allowed. You cannot use associative fields themselves as the value type of an associative field.

Associative fields can be used as function parameters and are then automatically transferred as a reference.

The size of an associative field can dynamically increase and decrease, depending on the number of its elements. Please bear in mind that, as a result, particularly the filling out of an associative field with new elements is not always fast.

You can determine the current number of elements in an associative field using the member function [size](../../../CAPLFunctions/AssociativeFields/Functions/CAPLfunctionSize.htm). Note that the [elcount](../../../CAPLFunctions/Other/Functions/CAPLfunctionElCount.htm) function always returns 1 when called with an associative field, because it determines the size of a non-associative field (array).

| ![Note](../../../../Resources/vImages/vInfo.png "Note") | Note<br>The use of associative fields is not compatible with older versions of CANoe. For this reason they may only be used from version 7.0 Service Pack 3 in CAPL programs. |
| ------------------------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |

## Working with an Initial Size

You can also specify an initial size in the declaration of the field:

int m[float, 30]; // initial space reserved for 30 elements

However, in the current implementation this does not usually lead to quantifiable speed gains.

| ![Note](../../../../Resources/vImages/vInfo.png "Note") | Note<br>Note also that memory amounting to the initial size is always occupied for the field at the start of the measurement (in the above example: 30 \* 3 bytes). Many fields whose initial size is high thus require a lot of memory even in the static state. |
| ------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |

## Element Access and Iteration

Elements of an associative field are accessed with square brackets, whereby the key is within the brackets:

value = m1[aKey];  
m2[aKey] = value + 1;

If the value of an element whose key is not yet in the field has to be changed, the key value pair is added to the field. That way the field can also be filled in a simple manner.

If an element whose key is not in the field has to be read out, a value initialized with 0 is inserted in the field and returned.

You can iterate all the elements of an associative field by using a variable of the key type with a special 'for' loop.

| ![Example](../../../../Resources/vImages/vExample.png "Example") | Example<br>void printMap(double m[long])   {    for (long aKey: m)    {    write("%d is mapped to %g.", aKey, m[aKey]);    }   } |
| ---------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------- |

The loop variable is only valid with the loop and can not be changed there. The iteration passes through the keys in the field in ascending order. Changing the field during iteration is permitted; newly added keys are only returned in the loop if they are greater than the current value of the loop variables.

## Functions

Each associative field provides special functions that can be called with the syntax Variable.FunctionName(Parameter):

* [clear](../../../CAPLFunctions/AssociativeFields/Functions/CAPLfunctionClear.htm)
* [containsKey](../../../CAPLFunctions/AssociativeFields/Functions/CAPLfunctionContainsKey.htm)
* [remove](../../../CAPLFunctions/AssociativeFields/Functions/CAPLfunctionRemove.htm)
* [size](../../../CAPLFunctions/AssociativeFields/Functions/CAPLfunctionSize.htm)

## Information about char[] as Key Type

If the key type char[] is specified, all the character fields (of any size) can be used as key values. In the iteration the loop variable must then also be declared as char[]. Key comparisons, e.g. in order to determine iteration sequence, are then performed as character string comparisons, whereby no country-specific algorithms are used.

char[] is the only field type that can be used as a key type. Please bear in mind that you can not declare variables or parameters of the char[] type, with the exception of loop variables in the iteration.

## Algorithmic Complexity

To enable you to estimate the duration of operations more easily when working with associative fields their algorithmic complexity will now be indicated in O-notation. If N is the number of elements in an associative field, the following applies:

* Searching for an element ([]-operator or function [containsKey](../../../CAPLFunctions/AssociativeFields/Functions/CAPLfunctionContainsKey.htm)) is in O(log(N)).
* Inserting elements is average in O(log(N)). In this case, average means that individual insertion operations may take longer if the internal data structure has to be resorted.
* Removing elements is in O(log(N)).
* Querying the element number (function [size](../../../CAPLFunctions/AssociativeFields/Functions/CAPLfunctionSize.htm)) is in O(1), i.e. constant time.
* Removing all elements (function [clear](../../../CAPLFunctions/AssociativeFields/Functions/CAPLfunctionClear.htm)) is in O(log(N) + N).
* The iteration via all the elements is in O(N \* log(N)) because in each iteration step an element is sought at least once.

| ![Example](../../../../Resources/vImages/vExample.png "Example") | Association between floating point numbers:<br>float m[float];   m[4.1] = 5.5; //key is 4.1 (float) and value is 5.5 (float)   m[5.3] = 6.6;   write ("4.1 is mapped to %2.2lf", m[4.1]);   write ("5.3 is mapped to %2.2lf", m[5.3]);    for (float mykey : m)   {    write("%2.2lf is mapped to %2.2lf.", mykey, m[mykey]);   }<br>Association between strings:<br>char[30] name[char []];   strncpy(name["Max"], "Mustermann", 30);    strncpy(name["Vector"], "Informatik", 30);    for (char[] mykey : name)   {    write("%s is mapped to %s", mykey, name[mykey]);   } |
| ---------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |

[Data Types for Variables](DataTypesForVariables.htm) â¢ [Data Types for Function Parameters](DataTypesForFunctionParameters.htm)

- [../../../Shared/HowToUseOnlineHelp.htm](../../../Shared/HowToUseOnlineHelp.htm "Tips for using the help") [](<> "Opens German help") [](<> "Opens English help") [](<> "Opens Japanese help")[../../../Shared/HowToUseOnlineHelp.htm#BMLanguage](../../../Shared/HowToUseOnlineHelp.htm#BMLanguage) [Vector Help Dashboard](https://help.vector.com/ "Opens the Vector Help Dashboard") â¢ [Version Selection](https://help.vector.com/CANoeDEFamily/index.html "Opens the version overview of your product") 2025-10-11T20:31:10

- Â© Vector Informatik GmbH CANoe (Desktop Editions & Test Bench Editions) Version 19.3.1 [Contact/Copyright/License](../../../Shared/ContactCopyrightLicense.htm) Cookie Settings [Data Privacy Notice](https://www.vector.com/int/en/company/get-info/privacy-policy/)