/*************************************************************************\
* Copyright (c) 2021 HZB.
* Author: Carsten Winkler carsten.winkler@helmholtz-berlin.de
*
* All you need to use custom data types is
*       UA_StatusCode initializeCustomDataTypes(UA_Client* client);
*
* How to read custom data types is shown in
*       UA_StatusCode UA_printValue(UA_Client* client, UA_NodeId nodeId,
*                                   UA_Variant* data, UA_String* output)
*
* You can find a working example in the main function
* --------------------------------------------------------------------------
* based on open62541
*   https://github.com/open62541/open62541/releases/tag/v1.2.2
*   used options: BUILD_SHARED_LIBS and UA_ENABLE_STATUSCODE_DESCRIPTIONS
*
* uses libxml2
*   Windows
*     git clone https://gitlab.gnome.org/GNOME/libxml2.git
*     cscript configure.js compiler=msvc debug=no iconv=no
*     nmake /f Makefile.msvc
*
*   Linux
*     sudo apt-get install libxml2
*     sudo apt-get install libxml2-dev(el)
*
\*************************************************************************/

#include <open62541/client_config_default.h>
#include <open62541/client_highlevel.h>
#include <open62541/plugin/log_stdout.h>

#include <libxml/parser.h> 
#include <libxml/tree.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>

#include <algorithm>
#include <map>
#include <cmath>
#include <sstream>
#include <string>
#include <vector>

#include "ExtendedObjectOpen62541.h"
#include "TailQueue.h"

#pragma warning(disable : 26812)
#define MEMORY_BANK_SIZE 4
//#undef UA_ENABLE_TYPEDESCRIPTION // for compatibility check only

static const UA_DataType* parseDataType(std::string text);
static const UA_UInt32 ADDRESS_SIZE = sizeof(void*);
static std::string byteStringToString(UA_ByteString* bytes);
static UA_Boolean isOptionSet(const UA_NodeId* subTypeNodeId);
static UA_PrintOutput* UA_PrintContext_addOutput(UA_PrintContext* ctx, size_t length);
UA_StatusCode parseXml(std::map<UA_UInt32, std::string>* dictionaries);
UA_StatusCode printUInt32(UA_PrintContext* ctx, UA_UInt32 p, UA_UInt32 width = 0, UA_Boolean isHex = false);
UA_StatusCode scan4BaseDataTypes(UA_Client* client);
UA_StatusCode UA_PrintContext_addNewlineTabs(UA_PrintContext* ctx, size_t tabs);
UA_StatusCode UA_PrintContext_addString(UA_PrintContext* ctx, const char* str);
void scanForTypeIds(UA_BrowseResponse* bResp, std::vector<UA_NodeId>* dataTypeIds, std::vector<UA_NodeId>* cutomDataTypeIds);
void UA_PrintTypeKind(UA_UInt32 typeKind, UA_String* out);

// https://www.programmingalgorithms.com/algorithm/sdbm-hash/cpp/
UA_UInt32 UA_ByteString_SDBMHash(UA_UInt32 fnv, const UA_Byte* buf, size_t size) {
    for (size_t i = 0; i < size; ++i) {
        fnv = buf[i] + (fnv << 6) + (fnv << 16) - fnv;
    }
    return fnv;
}

UA_UInt32 UA_NodeId_SDBMHash(const UA_NodeId* n) {
    switch (n->identifierType) {
    case UA_NODEIDTYPE_NUMERIC:
    default:
        return UA_ByteString_SDBMHash(n->namespaceIndex, (const UA_Byte*)&n->identifier.numeric,
            sizeof(UA_UInt32));
    case UA_NODEIDTYPE_STRING:
    case UA_NODEIDTYPE_BYTESTRING:
        return UA_ByteString_SDBMHash(n->namespaceIndex, n->identifier.string.data,
            n->identifier.string.length);
    case UA_NODEIDTYPE_GUID:
        return UA_ByteString_SDBMHash(n->namespaceIndex, (const UA_Byte*)&n->identifier.guid,
            sizeof(UA_Guid));
    }
}

// this function collects reference type ID, forwarded flag, 
// browse name, display name, node class and type definition
// returns true if browse was successful
UA_StatusCode browseNodeId(UA_Client* client, UA_NodeId nodeId, UA_BrowseResponse* bResp) {
    UA_BrowseRequest bReq;

    if (!client) {
        UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "browseNodeId: Client session invalid");
        return UA_STATUSCODE_BADUNEXPECTEDERROR;
    }
    if (!bResp) {
        UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "browseNodeId: Parameter 2 (UA_BrowseResponse*) invalid");
        return UA_STATUSCODE_BADUNEXPECTEDERROR;
    }
    UA_BrowseRequest_init(&bReq);
    bReq.requestedMaxReferencesPerNode = 0;
    bReq.nodesToBrowse = UA_BrowseDescription_new();
    bReq.nodesToBrowseSize = 1;
    UA_NodeId_copy(&nodeId, &bReq.nodesToBrowse[0].nodeId);
    bReq.nodesToBrowse[0].resultMask = UA_BROWSERESULTMASK_ALL; /* return everything */
    bReq.nodesToBrowse[0].browseDirection = UA_BROWSEDIRECTION_BOTH;
    *bResp = UA_Client_Service_browse(client, bReq);
    UA_BrowseRequest_clear(&bReq);
    return UA_STATUSCODE_GOOD;
}

// this function converts a byte string to a null terminated string
static std::string byteStringToString(UA_ByteString* bytes) {
    if (!bytes)
        return "";
    return std::string((char*)bytes->data, bytes->length);
}

// subfunction of calc_struct_padding
void sub_calc_struct_padding(UA_Byte bytes, UA_DataTypeMember* dataTypeMember, UA_UInt32* size, UA_Byte* maxVal, UA_Byte* currentMemoryBank, UA_Byte* padding) {
    if (bytes > *maxVal)
        *maxVal = bytes;
    if (bytes > 1 && *currentMemoryBank && bytes + *currentMemoryBank > ADDRESS_SIZE) {
        *padding = ADDRESS_SIZE - *currentMemoryBank;
    }
    else if (bytes > 1 && *currentMemoryBank % 4) {
        *padding = 4 - *currentMemoryBank;
    }
    *currentMemoryBank += bytes + *padding;
    *size += bytes + *padding;
    dataTypeMember->padding = *padding;
    while (*currentMemoryBank > ADDRESS_SIZE)
        *currentMemoryBank -= ADDRESS_SIZE;
    if (*currentMemoryBank == ADDRESS_SIZE)
        *currentMemoryBank = 0;
}

// calculates the additional memory consumption (padding)
// of data type members (structure members) in the RAM
// returns resulting RAM size of whole structure
UA_UInt32 calc_struct_padding(UA_DataType* dataType) {
    UA_Byte maxVal = 0;
    UA_UInt32 size = 0;
    UA_Byte bytes = 0;
    UA_Byte padding = 0;
    UA_Byte currentMemoryBank = 0;
    UA_DataTypeMember* dataTypeMember;

    for (UA_UInt32 i = 0; i < dataType->membersSize; i++) {
        dataTypeMember = &dataType->members[i];
        padding = 0;
        if (dataTypeMember->isOptional)
            bytes = ADDRESS_SIZE;
        else if (dataTypeMember->isArray) {
            bytes = sizeof(size_t);
            UA_Byte tmpMaxVal = maxVal;
            UA_Byte tmpCurrentMemoryBank = currentMemoryBank;
            UA_Byte tmpPadding = padding;
            sub_calc_struct_padding(bytes, dataTypeMember, &size, &maxVal, &currentMemoryBank, &padding);
            maxVal = tmpMaxVal;
            currentMemoryBank = tmpCurrentMemoryBank;
            padding = tmpPadding;
            bytes = ADDRESS_SIZE;
            sub_calc_struct_padding(bytes, dataTypeMember, &size, &maxVal, &currentMemoryBank, &padding);
        }
        else
            bytes = dataTypeMember->memberType->memSize;
        sub_calc_struct_padding(bytes, dataTypeMember, &size, &maxVal, &currentMemoryBank, &padding);
    }
    padding = 0;
    if (maxVal > MEMORY_BANK_SIZE) {
        while (currentMemoryBank % ADDRESS_SIZE) {
            padding++;
            currentMemoryBank++;
        }
    }
    else if (maxVal > 1) {
        while (currentMemoryBank % maxVal) {
            padding++;
            currentMemoryBank++;
        }
    }
    size += padding;
    return size;
}

// initializes the structure customTypeProperties_t
void customTypePropertiesInit(customTypeProperties_t* customTypeProperties, const UA_NodeId* customDataTypeId) {
    memset(&customTypeProperties->dataType, 0x0, sizeof(UA_DataType));
    UA_NodeId_copy(customDataTypeId, &customTypeProperties->dataType.typeId);
}

// source: https://stackoverflow.com/questions/23943728/case-insensitive-standard-string-comparison-in-c
static UA_Boolean exo_compare_pred(unsigned char a, unsigned char b) {
    return std::tolower(a) == std::tolower(b);
}

// case-insensitive string comparison (WARNING: this is not UTF-8 compatible)
static UA_Boolean exo_compare(std::string const& a, std::string const& b) {
    return a.length() == b.length() ? std::equal(b.begin(), b.end(), a.begin(), exo_compare_pred) : false;
}

// converts ASCII characters in string to lo upper case (WARNING: this is not UTF-8 compatible)
static std::string exo_toupper(std::string data) {
    std::for_each(data.begin(), data.end(), [](char& c) {
        c = ::toupper(c);
        });
    return data;
}

// converts ASCII characters in string to lo lower case (WARNING: this is not UTF-8 compatible)
static std::string exo_tolower(std::string data) {
    std::for_each(data.begin(), data.end(), [](char& c) {
        c = ::tolower(c);
        });
    return data;
}

// case-insensitive string search (WARNING: this is not UTF-8 compatible)
static size_t exo_find(std::string source, std::string search) {
    size_t pos;
    if (source.empty() || search.empty())
        return std::string::npos;
    pos = source.find(search);
    if (pos == std::string::npos)
        pos = source.find(exo_tolower(search));
    if (pos == std::string::npos)
        pos = source.find(exo_toupper(search));
    return pos;
}

// case-insensitive string search reverse (WARNING: this is not UTF-8 compatible)
static size_t exo_rfind(std::string source, std::string search) {
    size_t pos;
    if (source.empty() || search.empty())
        return std::string::npos;
    pos = source.rfind(search);
    if (pos == std::string::npos)
        pos = source.rfind(exo_tolower(search));
    if (pos == std::string::npos)
        pos = source.rfind(exo_toupper(search));
    return pos;
}

// retrieves all dictionaries of the OPC UA server
// the result map contains name space and raw XML content of dictionary
UA_StatusCode getDictionaries(UA_Client* client, std::map<UA_UInt32, std::string>* dictionaries) {
    UA_ReferenceDescription rDesc;
    UA_BrowseResponse bResp;
    UA_UInt16 nameSpaceIndex;
    UA_StatusCode retval;
    UA_Variant outValue;
    size_t found1, found2;
    std::map<UA_UInt32, std::string>::iterator dictionaryIt;

    if (!client) {
        UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "getDictionaries: Client session invalid");
        return UA_STATUSCODE_BADUNEXPECTEDERROR;
    }
    if (!dictionaries) {
        UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "getDictionaries: Parameter 2 (std::map<UA_UInt32, std::string>*) invalid");
        return UA_STATUSCODE_BADUNEXPECTEDERROR;
    }
    UA_LOG_DEBUG(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "getDictionaries: retrieve OPC UA dictionaries in progress ...");
    dictionaries->clear();
    retval = browseNodeId(client, UA_NODEID_NUMERIC(0, UA_NS0ID_OPCBINARYSCHEMA_TYPESYSTEM), &bResp);
    for (size_t i = 0; (retval == UA_STATUSCODE_GOOD) && i < bResp.resultsSize; ++i) {
        for (size_t j = 0; j < bResp.results[i].referencesSize; ++j) {
            rDesc = bResp.results[i].references[j];
            nameSpaceIndex = rDesc.nodeId.nodeId.namespaceIndex;
            if (nameSpaceIndex != 0) {
                retval = UA_Client_readValueAttribute(client, rDesc.nodeId.nodeId, &outValue);
                if ((retval == UA_STATUSCODE_GOOD) && (outValue.type == &UA_TYPES[UA_TYPES_BYTESTRING])) {
                    std::string rawDictionary = byteStringToString((UA_ByteString*)outValue.data);
                    dictionaryIt = dictionaries->find(nameSpaceIndex);
                    // add dictionary to new namespace
                    if (dictionaryIt == dictionaries->end()) {
                        dictionaries->insert(std::pair<UA_UInt16, std::string>(nameSpaceIndex, rawDictionary));
                    }
                    // append dictionary to known namespace
                    else {
                        found1 = exo_rfind(dictionaryIt->second, "</opc:TypeDictionary>");
                        if (found1 != std::string::npos) {
                            dictionaryIt->second = dictionaryIt->second.substr(0, found1);
                            found1 = exo_find(rawDictionary, "<opc:EnumeratedType");
                            found2 = exo_find(rawDictionary, "<opc:StructuredType");
                            if (found1 != std::string::npos && found2 != std::string::npos) {
                                if (found1 < found2)
                                    rawDictionary = rawDictionary.substr(found1);
                                else
                                    rawDictionary = rawDictionary.substr(found2);
                            }
                            else if (found1 != std::string::npos) {
                                rawDictionary = rawDictionary.substr(found1);
                            }
                            else if (found2 != std::string::npos) {
                                rawDictionary = rawDictionary.substr(found2);
                            }
                            dictionaryIt->second.append(rawDictionary);
                        }
                    }
                }
                UA_Variant_clear(&outValue);
            }
        }
    }
    UA_LOG_DEBUG(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "getDictionaries: finished");
    return retval;
}

// converts dictionary data type tag to OPC data type address (generated open62541 data types)
// checks for sub data types
const UA_DataType* getMemberDataType(std::string typeName) {
    const UA_DataType* memberDataType = 0x0;
    std::size_t found;
    if (typeName.empty())
        return memberDataType;
    memberDataType = parseDataType(typeName);
    if (!memberDataType) {
        found = typeName.find_first_of(':');
        if (found != std::string::npos)
            typeName = typeName.substr(found + 1);
        nameTypePropIt_t subTypePropIt = dataTypeNameMap.find(typeName);
        if (subTypePropIt != dataTypeNameMap.end())
            memberDataType = &subTypePropIt->second->dataType;
        else
            memberDataType = 0x0;
    }
    return memberDataType;
}

void getSubTypeProperties(UA_NodeId* subTypeNodeId, customTypeProperties_t* customTypeProperties) {
    // sub-type is a structure
    if (UA_NodeId_equal(subTypeNodeId, &NS0ID_STRUCTURE)) {
        customTypeProperties->dataType.typeKind = UA_DATATYPEKIND_STRUCTURE;
    }
    // sub-type is an option set
    else if (UA_NodeId_equal(subTypeNodeId, &NS0ID_OPTIONSET)) {
        customTypeProperties->dataType.membersSize = 2;
        customTypeProperties->dataType.members = (UA_DataTypeMember*)UA_malloc(customTypeProperties->dataType.membersSize * sizeof(UA_DataTypeMember));
        if (customTypeProperties->dataType.members) {
            memset(&customTypeProperties->dataType.members[0], 0x0, sizeof(UA_DataTypeMember));
            customTypeProperties->dataType.members[0].memberType = &UA_TYPES[UA_TYPES_BYTESTRING];
#ifdef UA_ENABLE_TYPEDESCRIPTION
            customTypeProperties->dataType.members[0].memberName = (char*)UA_malloc(6 * sizeof(char));
            if (customTypeProperties->dataType.members[0].memberName)
                strcpy((char*)customTypeProperties->dataType.members[0].memberName, "Value");
#endif
            memset(&customTypeProperties->dataType.members[1], 0x0, sizeof(UA_DataTypeMember));
            customTypeProperties->dataType.members[1].memberType = &UA_TYPES[UA_TYPES_BYTESTRING];
#ifdef UA_ENABLE_TYPEDESCRIPTION
            customTypeProperties->dataType.members[1].memberName = (char*)UA_malloc(10 * sizeof(char));
            if (customTypeProperties->dataType.members[1].memberName)
                strcpy((char*)customTypeProperties->dataType.members[1].memberName, "ValidBits");
#endif
        }
        else {
            UA_LOG_WARNING(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "getSubTypeProperties: could not allocate memory for DataTypeMember");
            customTypeProperties->dataType.membersSize = 0;
        }
        customTypeProperties->dataType.memSize = calc_struct_padding(&customTypeProperties->dataType);
        customTypeProperties->dataType.overlayable = UA_BINARY_OVERLAYABLE_INTEGER;
        customTypeProperties->dataType.pointerFree = true;
        customTypeProperties->dataType.typeKind = UA_DATATYPEKIND_STRUCTURE;
    }
    // sub-type is an UNION
    else if (UA_NodeId_equal(subTypeNodeId, &NS0ID_UNION)) {
        customTypeProperties->dataType.typeKind = UA_DATATYPEKIND_UNION;
    }
    // sub-type is an ENUMERATION
    else if (UA_NodeId_equal(subTypeNodeId, &NS0ID_ENUMERATION)) {
        customTypeProperties->dataType.typeKind = UA_DATATYPEKIND_ENUM;
        customTypeProperties->dataType.memSize = sizeof(UA_Int32);
        customTypeProperties->dataType.pointerFree = true;
        customTypeProperties->dataType.overlayable = true;
    }
}

// reads value of the named property at XML node
// name is case-insensitive (WARNING: name this is not UTF-8 compatible)
static std::string getXmlPropery(xmlNode* node, const char* name) {
    if (!node || !name)
        return std::string();
    char* propertyValue = 0x0;
    std::string retval;

    propertyValue = (char*)xmlGetProp(node, (const xmlChar*)name);
    if (!propertyValue)
        propertyValue = (char*)xmlGetProp(node, (const xmlChar*)exo_toupper(name).c_str());
    if (!propertyValue)
        propertyValue = (char*)xmlGetProp(node, (const xmlChar*)exo_tolower(name).c_str());
    if (propertyValue) {
        retval = std::string(propertyValue);
        xmlFree(propertyValue);
    }
    else {
        retval = std::string();
    }
    return retval;
}

// search for and implement custom data types of the server
UA_StatusCode initializeCustomDataTypes(UA_Client* client) {
    UA_StatusCode retval;
    std::map<UA_UInt32, std::string> dictionaries;
    std::map<UA_UInt32, xmlDocPtr> xmlDocMap;

    if (!client) {
        UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "initializeCustomDataTypes: Client session invalid");
        return UA_STATUSCODE_BADUNEXPECTEDERROR;
    }

    // retrieve custom data types from the server node /Types/DataTypes/BaseDataType
    retval = scan4BaseDataTypes(client);
    if (retval != UA_STATUSCODE_GOOD) {
        UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "initializeCustomDataTypes: Retrieve custom data types from the server node /Types/DataTypes/BaseDataType failed");
        return UA_STATUSCODE_BADUNEXPECTEDERROR;
    }

    // retrieve OPC UA dictionaries from the server
    retval = getDictionaries(client, &dictionaries);
    if (retval != UA_STATUSCODE_GOOD) {
        UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "initializeCustomDataTypes: Could not retrieve the OPC UA dictionary");
        return UA_STATUSCODE_BADUNEXPECTEDERROR;
    }

    retval = parseXml(&dictionaries);

    // initialize client with custom data types
    numberOfCustomDataTypes = (UA_UInt32)dataTypeMap.size();
    customDataTypes = (UA_DataTypeArray*)UA_malloc(numberOfCustomDataTypes * sizeof(UA_DataTypeArray));
    if (!customDataTypes) {
        numberOfCustomDataTypes = 0;
        return UA_STATUSCODE_BADOUTOFMEMORY;
    }
    memset((void*)customDataTypes, 0x0, numberOfCustomDataTypes * sizeof(UA_DataTypeArray));
    typePropIt_t dataTypeMapIt = dataTypeMap.begin();
    for (UA_UInt32 i = 0; dataTypeMapIt != dataTypeMap.end() && i < numberOfCustomDataTypes; dataTypeMapIt++) {
        if (UA_NodeId_equal(&dataTypeMapIt->second.dataType.typeId, &UA_NODEID_NULL))
            continue;
        customDataTypes[i].types = &dataTypeMapIt->second.dataType;
        *(size_t*)&(customDataTypes[i]).typesSize = 1;
        if ((i + 1) < numberOfCustomDataTypes)
            customDataTypes[i].next = &customDataTypes[i + 1];
        else
            customDataTypes[i].next = 0x0;
        i++;
    }
    // initialize current client session with new custom data types
    UA_Client_getConfig(client)->customDataTypes = customDataTypes;

    UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "initializeCustomDataTypes: Custom data types initialized");
    return retval;
}

// checks whether the SubTypeID is equal to the OptionSetID
static UA_Boolean isOptionSet(const UA_NodeId* subTypeNodeId) {
    return UA_NodeId_equal(subTypeNodeId, &NS0ID_OPTIONSET);
}

// converts dictionary data type tag to OPC data type address (generated open62541 data types)
static const UA_DataType* parseDataType(std::string text) {
    if (xmlStrncasecmp((const xmlChar*)("opc:String"), (xmlChar*)text.data(), (UA_UInt32)text.length()) == 0)
        return &(UA_TYPES[UA_TYPES_STRING]);
    if (xmlStrncasecmp((const xmlChar*)("opc:Byte"), (xmlChar*)text.data(), (UA_UInt32)text.length()) == 0)
        return &(UA_TYPES[UA_TYPES_BYTE]);
    if (xmlStrncasecmp((const xmlChar*)("opc:SByte"), (xmlChar*)text.data(), (UA_UInt32)text.length()) == 0)
        return &(UA_TYPES[UA_TYPES_SBYTE]);
    if (xmlStrncasecmp((const xmlChar*)("opc:Boolean"), (xmlChar*)text.data(), (UA_UInt32)text.length()) == 0)
        return &(UA_TYPES[UA_TYPES_BOOLEAN]);
    if (xmlStrncasecmp((const xmlChar*)("opc:Bit"), (xmlChar*)text.data(), (UA_UInt32)text.length()) == 0)
        return 0x0;// used for bit mask only
    if (xmlStrncasecmp((const xmlChar*)("opc:Int16"), (xmlChar*)text.data(), (UA_UInt32)text.length()) == 0)
        return &(UA_TYPES[UA_TYPES_INT16]);
    if (xmlStrncasecmp((const xmlChar*)("opc:Int32"), (xmlChar*)text.data(), (UA_UInt32)text.length()) == 0)
        return &(UA_TYPES[UA_TYPES_INT32]);
    if (xmlStrncasecmp((const xmlChar*)("opc:Int64"), (xmlChar*)text.data(), (UA_UInt32)text.length()) == 0)
        return &(UA_TYPES[UA_TYPES_INT64]);
    if (xmlStrncasecmp((const xmlChar*)("opc:UInt16"), (xmlChar*)text.data(), (UA_UInt32)text.length()) == 0)
        return &(UA_TYPES[UA_TYPES_UINT16]);
    if (xmlStrncasecmp((const xmlChar*)("opc:UInt32"), (xmlChar*)text.data(), (UA_UInt32)text.length()) == 0)
        return &(UA_TYPES[UA_TYPES_UINT32]);
    if (xmlStrncasecmp((const xmlChar*)("opc:UInt64"), (xmlChar*)text.data(), (UA_UInt32)text.length()) == 0)
        return &(UA_TYPES[UA_TYPES_UINT64]);
    if (xmlStrncasecmp((const xmlChar*)("opc:Float"), (xmlChar*)text.data(), (UA_UInt32)text.length()) == 0)
        return &(UA_TYPES[UA_TYPES_FLOAT]);
    if (xmlStrncasecmp((const xmlChar*)("opc:Double"), (xmlChar*)text.data(), (UA_UInt32)text.length()) == 0)
        return &(UA_TYPES[UA_TYPES_DOUBLE]);
    if (xmlStrncasecmp((const xmlChar*)("opc:DateTime"), (xmlChar*)text.data(), (UA_UInt32)text.length()) == 0)
        return &(UA_TYPES[UA_TYPES_DATETIME]);
    if (xmlStrncasecmp((const xmlChar*)("opc:Guid"), (xmlChar*)text.data(), (UA_UInt32)text.length()) == 0)
        return &(UA_TYPES[UA_TYPES_GUID]);
    if (xmlStrncasecmp((const xmlChar*)("opc:ByteString"), (xmlChar*)text.data(), (UA_UInt32)text.length()) == 0)
        return &(UA_TYPES[UA_TYPES_BYTESTRING]);
    if (xmlStrncasecmp((const xmlChar*)("ua:XmlElement"), (xmlChar*)text.data(), (UA_UInt32)text.length()) == 0)
        return &(UA_TYPES[UA_TYPES_XMLELEMENT]);
    if (xmlStrncasecmp((const xmlChar*)("ua:NodeId"), (xmlChar*)text.data(), (UA_UInt32)text.length()) == 0)
        return &(UA_TYPES[UA_TYPES_NODEID]);
    if (xmlStrncasecmp((const xmlChar*)("ua:ExpandedNodeId"), (xmlChar*)text.data(), (UA_UInt32)text.length()) == 0)
        return &(UA_TYPES[UA_TYPES_EXPANDEDNODEID]);
    if (xmlStrncasecmp((const xmlChar*)("ua:QualifiedName"), (xmlChar*)text.data(), (UA_UInt32)text.length()) == 0)
        return &(UA_TYPES[UA_TYPES_QUALIFIEDNAME]);
    if (xmlStrncasecmp((const xmlChar*)("ua:LocalizedText"), (xmlChar*)text.data(), (UA_UInt32)text.length()) == 0)
        return &(UA_TYPES[UA_TYPES_LOCALIZEDTEXT]);
    if (xmlStrncasecmp((const xmlChar*)("ua:StatusCode"), (xmlChar*)text.data(), (UA_UInt32)text.length()) == 0)
        return &(UA_TYPES[UA_TYPES_STATUSCODE]);
    if (xmlStrncasecmp((const xmlChar*)("ua:Variant"), (xmlChar*)text.data(), (UA_UInt32)text.length()) == 0)
        return &(UA_TYPES[UA_TYPES_VARIANT]);
    if (xmlStrncasecmp((const xmlChar*)("ua:Int32"), (xmlChar*)text.data(), (UA_UInt32)text.length()) == 0)
        return &(UA_TYPES[UA_TYPES_INT32]);
    if (xmlStrncasecmp((const xmlChar*)("ua:ExtensionObject"), (xmlChar*)text.data(), (UA_UInt32)text.length()) == 0)
        return &(UA_TYPES[UA_TYPES_EXTENSIONOBJECT]);
    if (xmlStrncasecmp((const xmlChar*)("ua:DiagnosticInfo"), (xmlChar*)text.data(), (UA_UInt32)text.length()) == 0)
        return &(UA_TYPES[UA_TYPES_DIAGNOSTICINFO]);
    if (xmlStrncasecmp((const xmlChar*)("opc:CharArray"), (xmlChar*)text.data(), (UA_UInt32)text.length()) == 0)
        return &(UA_TYPES[UA_TYPES_STRING]);
    return 0x0;
}

// reads xml nodes of the dictionary and extracts a data structure including its members
// and calculates memory padding in data structure for RAM instances
UA_StatusCode parseXml(std::map<UA_UInt32, std::string>* dictionaries) {
    if (!dictionaries) {
        UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "getXmlDocMap: Parameter 1 (std::map<UA_UInt32, std::string>*) invalid");
        return UA_STATUSCODE_BADUNEXPECTEDERROR;
    }
    const UA_UInt16 xmlPathCount = 2;
    const char xmlPath[xmlPathCount][40] = { "/opc:TypeDictionary/opc:StructuredType" , "/opc:TypeDictionary/opc:EnumeratedType" };
    char** tmp, * pError;
    const UA_DataType* memberDataType;
    UA_DataTypeMember* member;
    nameTypePropIt_t typePropIt;
    std::map<UA_UInt32, std::string>::iterator it;
    std::string browseName;
    std::string lengthField;
    std::string name;
    std::string propertyName;
    std::string propertyType;
    std::string propertyValue;
    std::string switchField;
    std::string switchValue;
    std::string typeName;
    std::string val;
    std::string value;
    std::vector<UA_DataTypeMember> structMemberTypes;
    UA_Boolean isArray;
    UA_Boolean isOptional;
    UA_DataType* dataType;
    UA_UInt32 maxSize;
    xmlDocPtr doc;
    xmlXPathContextPtr xpathCtx;
    xmlXPathObjectPtr xpathObj;

    // collects custom data type properties of all specified dictionaries
    for (it = dictionaries->begin(); it != dictionaries->end(); ++it) {
        // parse the current dictionary and build a node tree
        doc = xmlReadMemory((const char*)it->second.c_str(), (UA_UInt32)it->second.length(), "include.xml", 0x0, 0);
        if (!doc) {
            UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "parseXml: Could not read all dictionaries");
            return UA_STATUSCODE_BADUNEXPECTEDERROR;
        }
        // create a new context of XPath
        xpathCtx = xmlXPathNewContext(doc);
        if (!xpathCtx) {
            UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "parseXml: Create new XPath instance failed");
            return UA_STATUSCODE_BADUNEXPECTEDERROR;
        }
        // register namespace "opc"
        if (xmlXPathRegisterNs(xpathCtx, BAD_CAST "opc", BAD_CAST "http://opcfoundation.org/BinarySchema/")) {
            UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "parseXml: Registration of the new namespace \"opc\" of XPath failed");
            return UA_STATUSCODE_BADUNEXPECTEDERROR;
        }
        // process all specified XML paths
        for (UA_UInt16 i = 0; i < xmlPathCount; i++) {
            // evaluate current XML path
            xpathObj = xmlXPathEvalExpression(BAD_CAST xmlPath[i], xpathCtx);
            if (!xpathObj) {
                UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "parseXml: XPath evaluation function failed for %s.", xmlPath[i]);
                return UA_STATUSCODE_BADUNEXPECTEDERROR;
            }
            if (!xpathObj->nodesetval)
                continue;
            for (UA_UInt16 j = 0; j < xpathObj->nodesetval->nodeNr; j++) {
                xmlNode* node = xpathObj->nodesetval->nodeTab[j];
                xmlNode* children = node->children;
                isArray = false;
                isOptional = false;
                propertyType = (char*)node->name;
                browseName = (char*)xmlGetProp(node, BAD_CAST "Name");
                typePropIt = dataTypeNameMap.find(browseName);
                if (typePropIt == dataTypeNameMap.end()) {
                    UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "parseXml: custom data type %s not found in the node branch DataTypes", browseName.c_str());
                    continue;
                }
                dataType = &typePropIt->second->dataType;
                structMemberTypes.clear();
                // processing of OptionSets
                if (isOptionSet(&typePropIt->second->subTypeOfId)) {
                    // nothing to do
                }
                // processing of structures information of the XML node such as
                // simple structures, structures with optional fields, and unions
                else if (exo_compare(propertyType, "StructuredType")) {
                    // check for structure with optional fields
                    typeName.clear();
                    switchField.clear();
                    while (children) {
                        if (children->type != XML_ELEMENT_NODE || !exo_compare((char*)children->name, "Field")) {
                            children = children->next;
                            continue;
                        }
                        if (typeName.empty()) {
                            val = getXmlPropery(children, "TypeName");
                            if (!val.empty() && exo_compare(val, "opc:Bit"))
                                typeName = val;
                        }
                        if (switchField.empty()) {
                            val = getXmlPropery(children, "SwitchField");
                            if (!val.empty())
                                switchField = val;
                        }
                        if (!typeName.empty() && !switchField.empty()) {
                            dataType->typeKind = UA_DATATYPEKIND_OPTSTRUCT;
                            break;
                        }
                        children = children->next;
                    }
                    children = node->children;
                    // processing of simple structures
                    if (dataType->typeKind == UA_DATATYPEKIND_STRUCTURE) {
                        while (children) {
                            isArray = false;
                            lengthField.clear();
                            name.clear();
                            typeName.clear();
                            if (children->type != XML_ELEMENT_NODE || !exo_compare((char*)children->name, "Field")) {
                                children = children->next;
                                continue;
                            }
                            val = getXmlPropery(children, "Name");
                            if (!val.empty())
                                name = val;
                            val = getXmlPropery(children, "TypeName");
                            if (!val.empty())
                                typeName = val;
                            val = getXmlPropery(children, "LengthField");
                            if (!val.empty())
                                lengthField = val;
                            if (!lengthField.empty())
                                isArray = true;
                            if (!typeName.empty())
                                memberDataType = getMemberDataType(typeName);
                            else
                                memberDataType = 0x0;
                            if (!memberDataType) {
                                UA_LOG_WARNING(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "parseXml: %s::%s not found", browseName.c_str(), typeName.c_str());
                                children = children->next;
                                continue;
                            }
                            if (!memberDataType->pointerFree || (memberDataType->typeId.namespaceIndex == 0 && memberDataType->typeId.identifierType == UA_NODEIDTYPE_NUMERIC && memberDataType->typeId.identifier.numeric > UA_TYPES_DOUBLE))
                                dataType->pointerFree = false;
                            UA_DataTypeMember dataTypeMember;
                            memset(&dataTypeMember, 0x0, sizeof(UA_DataTypeMember));
                            dataTypeMember.memberType = memberDataType;
 #ifdef UA_ENABLE_TYPEDESCRIPTION
                            if (!dataTypeMember.memberName) {
                                tmp = (char**)&dataTypeMember.memberName;
                                *tmp = (char*)UA_malloc(name.length() * sizeof(char) + 1);
                                if (*tmp)
                                    strcpy(*tmp, name.c_str());
                            }
#endif
                            dataTypeMember.isOptional = false;
                            dataTypeMember.isArray = isArray;
                            structMemberTypes.push_back(dataTypeMember);
                            children = children->next;
                        }
                    }
                    // processing of structures with optional fields
                    else if (dataType->typeKind == UA_DATATYPEKIND_OPTSTRUCT) {
                        while (children) {
                            isArray = false;
                            isOptional = false;
                            lengthField.clear();
                            name.clear();
                            switchField.clear();
                            typeName.clear();
                            if (children->type != XML_ELEMENT_NODE || !exo_compare((char*)children->name, "Field")) {
                                children = children->next;
                                continue;
                            }
                            val = getXmlPropery(children, "TypeName");
                            if (!val.empty()) {
                                typeName = val;
                                if (exo_compare(typeName, "opc:Bit")) {
                                    children = children->next;
                                    continue;
                                }
                            }
                            val = getXmlPropery(children, "Name");
                            if (!val.empty())
                                name = val;
                            val = getXmlPropery(children, "SwitchField");
                            if (!val.empty())
                                switchField = val;
                            val = getXmlPropery(children, "LengthField");
                            if (!val.empty())
                                lengthField = val;
                            if (!switchField.empty())
                                isOptional = true;
                            if (!lengthField.empty())
                                isArray = true;
                            if (!typeName.empty())
                                memberDataType = getMemberDataType(typeName);
                            else
                                memberDataType = 0x0;
                            if (!memberDataType) {
                                UA_LOG_WARNING(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "parseXml: %s::%s not found", browseName.c_str(), typeName.c_str());
                                children = children->next;
                                continue;
                            }
                            if (!memberDataType->pointerFree || (memberDataType->typeId.namespaceIndex == 0 && memberDataType->typeId.identifierType == UA_NODEIDTYPE_NUMERIC && memberDataType->typeId.identifier.numeric > UA_TYPES_DOUBLE))
                                dataType->pointerFree = false;
                            UA_DataTypeMember dataTypeMember;
                            memset(&dataTypeMember, 0x0, sizeof(UA_DataTypeMember));
                            dataTypeMember.memberType = memberDataType;
#ifdef UA_ENABLE_TYPEDESCRIPTION
                            tmp = (char**)&dataTypeMember.memberName;
                            *tmp = (char*)UA_malloc(name.length() * sizeof(char) + 1);
                            if (*tmp)
                                strcpy(*tmp, name.c_str());
#endif
                            dataTypeMember.isOptional = isOptional;
                            dataTypeMember.isArray = isArray;
                            structMemberTypes.push_back(dataTypeMember);
                            children = children->next;
                        }
                    }
                    // processing of UNION-structures
                    else if (dataType->typeKind == UA_DATATYPEKIND_UNION) {
                        while (children) {
                            isArray = false;
                            lengthField.clear();
                            name.clear();
                            switchField.clear();
                            switchValue.clear();
                            typeName.clear();
                            if (children->type != XML_ELEMENT_NODE || !exo_compare((char*)children->name, "Field")) {
                                children = children->next;
                                continue;
                            }
                            val = getXmlPropery(children, "TypeName");
                            if (!val.empty()) {
                                typeName = val;
                                if (exo_compare(typeName, "opc:Bit")) {
                                    children = children->next;
                                    continue;
                                }
                            }
                            val = getXmlPropery(children, "Name");
                            if (!val.empty())
                                name = val;
                            val = getXmlPropery(children, "SwitchField");
                            if (!val.empty())
                                switchField = val;
                            val = getXmlPropery(children, "SwitchValue");
                            if (!val.empty())
                                switchValue = val;
                            if (!lengthField.empty())
                                isArray = true;
                            if (!typeName.empty())
                                memberDataType = getMemberDataType(typeName);
                            else
                                memberDataType = 0x0;
                            if (!memberDataType) {
                                UA_LOG_WARNING(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "parseXml: %s::%s not found", browseName.c_str(), typeName.c_str());
                                children = children->next;
                                continue;
                            }
                            if (!memberDataType->pointerFree || (memberDataType->typeId.namespaceIndex == 0 && memberDataType->typeId.identifierType == UA_NODEIDTYPE_NUMERIC && memberDataType->typeId.identifier.numeric > UA_TYPES_DOUBLE))
                                dataType->pointerFree = false;
                            UA_DataTypeMember dataTypeMember;
                            memset(&dataTypeMember, 0x0, sizeof(UA_DataTypeMember));
                            dataTypeMember.memberType = memberDataType;
#ifdef UA_ENABLE_TYPEDESCRIPTION
                            tmp = (char**)&dataTypeMember.memberName;
                            *tmp = (char*)UA_malloc(name.length() * sizeof(char) + 1);
                            if (*tmp)
                                strcpy(*tmp, name.c_str());
#endif
                            dataTypeMember.isOptional = isOptional;
                            dataTypeMember.isArray = isArray;
                            structMemberTypes.push_back(dataTypeMember);
                            children = children->next;
                        }
                    }
                    /*else {
                        UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "parseXml: custom data type %s structure not found in the node branch DataTypes", browseName.c_str());
                    }*/
                }
                // processing of enumerations if no declaration was previously available
                else if (exo_compare(propertyType, "EnumeratedType") && typePropIt->second->enumValueSet.empty()) {
                    while (children) {
                        UA_EnumValueType enumValue;
                        UA_EnumValueType_init(&enumValue);
                        name.clear();
                        value.clear();
                        if (children->type != XML_ELEMENT_NODE || !exo_compare((char*)children->name, "EnumeratedValue")) {
                            children = children->next;
                            continue;
                        }
                        val = getXmlPropery(children, "Name");
                        if (!val.empty()) {
                            name = val;
                        }
                        val = getXmlPropery(children, "Value");
                        if (!val.empty()) {
                            value = val;
                        }
                        if (!name.empty() && !value.empty()) {
                            long i = strtol(val.c_str(), &pError, 10);
                            if (val.c_str() != pError) {
                                enumValue.value = i;
                                enumValue.description.text = UA_STRING_ALLOC(name.c_str());
                                enumValue.displayName.text = UA_STRING_ALLOC(name.c_str());
                                typePropIt->second->enumValueSet.push_back(enumValue);
                            }
                        }
                        children = children->next;
                    }
                }
                // finalize completion of the custom data type
                if (!structMemberTypes.empty()) {
                    // convert structure arrays to valid format first
                    if (dataType->typeKind == UA_DATATYPEKIND_STRUCTURE || dataType->typeKind == UA_DATATYPEKIND_OPTSTRUCT) {
                        std::vector<UA_DataTypeMember>::iterator prevMmemberIt;
                        // trim arrays (opc:Int32 + opc:DATATYPE => opc:DATATYPE)
                        for (std::vector<UA_DataTypeMember>::iterator memberIt = structMemberTypes.begin(); memberIt != structMemberTypes.end();) {
                            if (memberIt->isArray && prevMmemberIt != structMemberTypes.end()) {
                                memberIt = structMemberTypes.erase(prevMmemberIt);
                                if (memberIt != structMemberTypes.end()) {
                                    prevMmemberIt = structMemberTypes.end();
                                    memberIt++;
                                }
                            }
                            else {
                                prevMmemberIt = memberIt;
                                memberIt++;
                            }
                        }
                    }
                    // create missing DataTypeMembers
                    if (!dataType->members) {
                        dataType->membersSize = structMemberTypes.size();
                        dataType->members = (UA_DataTypeMember*)UA_malloc(dataType->membersSize * sizeof(UA_DataTypeMember));
                        memset(dataType->members, 0x0, dataType->membersSize * sizeof(UA_DataTypeMember));
                    }
                    // filling in the user data type
                    if (dataType->members) {
                        // UNION
                        if (dataType->typeKind == UA_DATATYPEKIND_UNION && structMemberTypes.size() > 1) {
                            UA_DataType memoryCheckUnion;
                            memset(&memoryCheckUnion, 0x0, sizeof(UA_DataType));
                            memoryCheckUnion.membersSize = 2;
                            memoryCheckUnion.members = (UA_DataTypeMember*)UA_malloc(memoryCheckUnion.membersSize * sizeof(UA_DataTypeMember));
                            if (!memoryCheckUnion.members) {
                                UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "parseXml: could not allocate memory for data members of memoryCheckUnion");
                                return UA_STATUSCODE_BADOUTOFMEMORY;
                            }
                            memset(memoryCheckUnion.members, 0x0, memoryCheckUnion.membersSize * sizeof(UA_DataTypeMember));
                            memoryCheckUnion.members[0].memberType = structMemberTypes.at(0).memberType;
                            dataType->membersSize = structMemberTypes.size() - 1;
                            UA_DataTypeMember* tmp = (UA_DataTypeMember*)UA_realloc(dataType->members, dataType->membersSize * sizeof(UA_DataTypeMember));
                            if (!tmp) {
                                UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "parseXml: could not allocate memory for data members of %s", browseName.c_str());
                                return UA_STATUSCODE_BADOUTOFMEMORY;
                            }
                            dataType->members = tmp;
                            memset(dataType->members, 0x0, dataType->membersSize * sizeof(UA_DataTypeMember));
                            maxSize = 0;
                            for (UA_UInt32 i = 1; i < structMemberTypes.size(); i++) {
                                dataType->members[i - 1].isArray = structMemberTypes.at(i).isArray;
                                dataType->members[i - 1].isOptional = structMemberTypes.at(i).isOptional;
#ifdef UA_ENABLE_TYPEDESCRIPTION
                                if (*structMemberTypes.at(i).memberName) {
                                    dataType->members[i - 1].memberName = (char*)UA_malloc(strlen(structMemberTypes.at(i).memberName) * sizeof(char) + 1);
                                    strcpy((char*)dataType->members[i - 1].memberName, structMemberTypes.at(i).memberName);
                                }
#endif
                                dataType->members[i - 1].memberType = structMemberTypes.at(i).memberType;
                                dataType->members[i - 1].padding = 0;
                                if (dataType->members[i - 1].memberType->memSize > maxSize) {
                                    maxSize = dataType->members[i - 1].memberType->memSize;
                                    memoryCheckUnion.members[1].memberType = dataType->members[i - 1].memberType;
                                }
                            }
                            dataType->memSize = calc_struct_padding(&memoryCheckUnion);
                            for (UA_UInt32 i = 1; i < structMemberTypes.size(); i++)
                                dataType->members[i - 1].padding = memoryCheckUnion.members[0].memberType->memSize + memoryCheckUnion.members[1].padding;
                        }
                        // STRUCTURE and STRUCTURE WITH OPTIONAL FIELDS
                        else if (dataType->typeKind == UA_DATATYPEKIND_STRUCTURE || dataType->typeKind == UA_DATATYPEKIND_OPTSTRUCT) {
                            for (UA_UInt32 i = 0; i < structMemberTypes.size(); i++) {
                                member = &structMemberTypes.at(i);
                                dataType->members[i].isOptional = member->isOptional;
                                dataType->members[i].isArray = member->isArray;
                                dataType->members[i].memberType = member->memberType;
#ifdef UA_ENABLE_TYPEDESCRIPTION
                                if (member->memberName) {
                                    dataType->members[i].memberName = (char*)UA_malloc(strlen(member->memberName) * sizeof(char*) + 1);
                                    if (dataType->members[i].memberName)
                                        strcpy((char*)dataType->members[i].memberName, member->memberName);
                                }
#endif
                            }
                            dataType->memSize = calc_struct_padding(dataType);
                        }
                    }
                    else {
                        UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "parseXml: could not allocate memory for data type members of %s", browseName.c_str());
                        dataType->membersSize = 0;
                    }
                }
            }
        }
    }
    return UA_STATUSCODE_GOOD;
}

// copy of open62541/src/ua_types_print.c
UA_StatusCode printArray(UA_PrintContext* ctx, const void* p, const size_t length, const UA_DataType* type) {
    UA_StatusCode retval;
    UA_PrintOutput* out;
    UA_String outString;

    retval = UA_STATUSCODE_GOOD;
    if (!p) {
        retval |= UA_PrintContext_addString(ctx, "(");
        retval |= UA_PrintContext_addString(ctx, type->typeName);
        retval |= UA_PrintContext_addString(ctx, " [empty])");
        return retval;
    }

    UA_UInt32 length32 = (UA_UInt32)length;
    retval |= UA_PrintContext_addString(ctx, "(");
    retval |= UA_PrintContext_addString(ctx, type->typeName);
    retval |= UA_PrintContext_addString(ctx, "[");
    retval |= printUInt32(ctx, length32);
    retval |= UA_PrintContext_addString(ctx, "]) {");
    ctx->depth++;
    uintptr_t target = (uintptr_t)p;
    for (UA_UInt32 i = 0; i < length; i++) {
        UA_PrintContext_addNewlineTabs(ctx, ctx->depth);
        printUInt32(ctx, i);
        retval |= UA_PrintContext_addString(ctx, ": ");
        UA_print((const void*)target, type, &outString);
        out = UA_PrintContext_addOutput(ctx, outString.length);
        if (!out)
            retval |= UA_STATUSCODE_BADOUTOFMEMORY;
        else
            memcpy(&out->data, outString.data, outString.length);
        UA_String_clear(&outString);
        if (i < length - 1)
            retval |= UA_PrintContext_addString(ctx, ",");
        target += type->memSize;
    }
    ctx->depth--;
    UA_PrintContext_addNewlineTabs(ctx, ctx->depth);
    retval |= UA_PrintContext_addString(ctx, "}");
    return retval;
}

// copy of open62541/src/ua_types_print.c
// modifications: new optional parameters width (leading white spaces) and isHex (print value 
UA_StatusCode printUInt32(UA_PrintContext* ctx, UA_UInt32 p, UA_UInt32 width, UA_Boolean isHex) {
    char out[32];
    if (isHex)
        snprintf(out, 32, "%0*x", width, p);
    else
        snprintf(out, 32, "%0*u", width, p);
    return UA_PrintContext_addString(ctx, out);
}

// copy of open62541/src/ua_types_print.c
UA_StatusCode UA_PrintContext_addName(UA_PrintContext* ctx, const char* name) {
    if (!name) {
        UA_PrintOutput* out = UA_PrintContext_addOutput(ctx, 3);
        if (!out)
            return UA_STATUSCODE_BADOUTOFMEMORY;
        memcpy(&out->data, "???", 3);
        return UA_STATUSCODE_GOOD;
    }
    else {
        size_t nameLen = strlen(name);
        UA_PrintOutput* out = UA_PrintContext_addOutput(ctx, nameLen + 2);
        if (!out)
            return UA_STATUSCODE_BADOUTOFMEMORY;
        memcpy(&out->data, name, nameLen);
        out->data[nameLen] = ':';
        out->data[nameLen + 1] = ' ';
        return UA_STATUSCODE_GOOD;
    }
}

// copy of open62541/src/ua_types_print.c
static UA_PrintOutput* UA_PrintContext_addOutput(UA_PrintContext* ctx, size_t length) {
    /* Protect against overlong output in pretty-printing */
    if (length > 2 << 16)
        return 0x0;
    UA_PrintOutput* output = (UA_PrintOutput*)UA_malloc(sizeof(UA_PrintOutput) + length + 1);
    if (!output)
        return 0x0;
    output->length = length;
    TAILQ_INSERT_TAIL(&ctx->outputs, output, next);
    return output;
}

// copy of open62541/src/ua_types_print.c
UA_StatusCode UA_PrintContext_addNewlineTabs(UA_PrintContext* ctx, size_t tabs) {
    UA_PrintOutput* out = UA_PrintContext_addOutput(ctx, tabs + 1);
    if (!out)
        return UA_STATUSCODE_BADOUTOFMEMORY;
    out->data[0] = '\n';
    for (size_t i = 1; i <= tabs; i++)
        out->data[i] = '\t';
    return UA_STATUSCODE_GOOD;
}

// copy of open62541/src/ua_types_print.c
UA_StatusCode UA_PrintContext_addString(UA_PrintContext* ctx, const char* str) {
    if (!str) {
        UA_PrintOutput* out = UA_PrintContext_addOutput(ctx, 3);
        if (!out)
            return UA_STATUSCODE_BADOUTOFMEMORY;
        memcpy(&out->data, "???", 3);
        return UA_STATUSCODE_GOOD;
    }
    else {
        size_t len = strlen(str);
        UA_PrintOutput* out = UA_PrintContext_addOutput(ctx, len);
        if (!out)
            return UA_STATUSCODE_BADOUTOFMEMORY;
        memcpy(&out->data, str, len);
        return UA_STATUSCODE_GOOD;
    }
}

// copy of open62541/src/ua_types_print.c
UA_StatusCode UA_PrintContext_addUAString(UA_PrintContext* ctx, UA_String* str) {
    UA_PrintOutput* out = UA_PrintContext_addOutput(ctx, str->length);
    if (!out)
        return UA_STATUSCODE_BADOUTOFMEMORY;
    memcpy(&out->data, str->data, str->length);
    return UA_STATUSCODE_GOOD;
}

// prints custom data type map to UA_String
UA_StatusCode UA_PrintCustomDataTypeMap(UA_String* output) {
    UA_PrintContext ctx{};
    UA_StatusCode retval;
    UA_String out;
    if (!output) {
        UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "UA_PrintCustomDataTypeMap: Parameter 1 (UA_String*) invalid");
        return UA_STATUSCODE_BADUNEXPECTEDERROR;
    }
    retval = UA_STATUSCODE_GOOD;
    ctx.depth = 0;
    TAILQ_INIT(&ctx.outputs);
    UA_String_init(output);
    retval |= UA_PrintContext_addNewlineTabs(&ctx, ctx.depth);
    retval |= UA_PrintContext_addString(&ctx, "***************************** DATA TYPE MAP BEGIN *****************************");
    retval |= UA_PrintContext_addNewlineTabs(&ctx, ctx.depth);
    for (std::pair<const UA_UInt32, customTypeProperties_t>& typeProp : dataTypeMap) {
        retval |= UA_PrintDataType(&typeProp.second.dataType, &out);
        retval |= UA_PrintContext_addUAString(&ctx, &out);
        UA_String_clear(&out);
        retval |= UA_PrintContext_addNewlineTabs(&ctx, ctx.depth);
    }
    retval |= UA_PrintContext_addString(&ctx, "****************************** DATA TYPE MAP END ******************************");
    /* Allocate memory for the output */
    if (retval == UA_STATUSCODE_GOOD) {
        size_t total = 0;
        UA_PrintOutput* out;
        TAILQ_FOREACH(out, &ctx.outputs, next)
            total += out->length;
        retval = UA_ByteString_allocBuffer((UA_String*)output, total);
    }
    /* Write the output */
    if (retval == UA_STATUSCODE_GOOD) {
        size_t pos = 0;
        UA_PrintOutput* out;
        TAILQ_FOREACH(out, &ctx.outputs, next) {
            memcpy(&output->data[pos], out->data, out->length);
            pos += out->length;
        }
    }
    /* Free the context */
    UA_PrintOutput* o, * o2;
    TAILQ_FOREACH_SAFE(o, &ctx.outputs, next, o2) {
        TAILQ_REMOVE(&ctx.outputs, o, next);
        UA_free(o);
    }
    return retval;
}

// prints data type structure to UA_String
UA_StatusCode UA_PrintDataType(const UA_DataType* dataType, UA_String* output) {
    UA_PrintContext ctx{};
    UA_StatusCode retval;
    UA_String out;
    UA_UInt32 size;

    if (!dataType) {
        UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "UA_PrintDataType: Parameter 1 (UA_DataType*) invalid");
        return UA_STATUSCODE_BADUNEXPECTEDERROR;
    }
    if (!output) {
        UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "UA_PrintDataType: Parameter 2 (UA_String*) invalid");
        return UA_STATUSCODE_BADUNEXPECTEDERROR;
    }
    retval = UA_STATUSCODE_GOOD;
    ctx.depth = 0;
    TAILQ_INIT(&ctx.outputs);
    UA_String_init(output);
#ifdef UA_ENABLE_TYPEDESCRIPTION
    UA_PrintContext_addName(&ctx, dataType->typeName);
#endif
    retval |= UA_PrintContext_addString(&ctx, "\n{");
    ctx.depth++;
    retval |= UA_PrintContext_addNewlineTabs(&ctx, ctx.depth);
    retval |= UA_PrintContext_addString(&ctx, "NodeId: ");
    retval |= UA_print(&dataType->typeId, &UA_TYPES[UA_TYPES_NODEID], &out);
    retval |= UA_PrintContext_addUAString(&ctx, &out);
    UA_String_clear(&out);

    retval |= UA_PrintContext_addNewlineTabs(&ctx, ctx.depth);
    retval |= UA_PrintContext_addString(&ctx, "BinaryNodeId: ");
    retval |= UA_print(&dataType->binaryEncodingId, &UA_TYPES[UA_TYPES_NODEID], &out);
    retval |= UA_PrintContext_addUAString(&ctx, &out);
    UA_String_clear(&out);

    retval |= UA_PrintContext_addNewlineTabs(&ctx, ctx.depth);
    retval |= UA_PrintContext_addString(&ctx, "Type Kind: ");
    UA_PrintTypeKind(dataType->typeKind, &out);
    retval |= UA_PrintContext_addUAString(&ctx, &out);
    UA_String_clear(&out);

    retval |= UA_PrintContext_addNewlineTabs(&ctx, ctx.depth);
    retval |= UA_PrintContext_addString(&ctx, "Memory Size: ");
    size = dataType->memSize;
    retval |= UA_print(&size, &UA_TYPES[UA_TYPES_UINT16], &out);
    retval |= UA_PrintContext_addUAString(&ctx, &out);
    UA_String_clear(&out);
    retval |= UA_PrintContext_addString(&ctx, " bytes");

    retval |= UA_PrintContext_addNewlineTabs(&ctx, ctx.depth);
    retval |= UA_PrintContext_addString(&ctx, "Pointer Free: ");
    retval |= UA_PrintContext_addString(&ctx, dataType->pointerFree ? "TRUE" : "FALSE");

    retval |= UA_PrintContext_addNewlineTabs(&ctx, ctx.depth);
    retval |= UA_PrintContext_addString(&ctx, "Overlayable: ");
    retval |= UA_PrintContext_addString(&ctx, dataType->overlayable ? "TRUE" : "FALSE");

    retval |= UA_PrintContext_addNewlineTabs(&ctx, ctx.depth);
    retval |= UA_PrintContext_addString(&ctx, "Number Of Members: ");
    size = dataType->membersSize;
    retval |= UA_print(&size, &UA_TYPES[UA_TYPES_UINT16], &out);
    retval |= UA_PrintContext_addUAString(&ctx, &out);
    UA_String_clear(&out);

    if (size) {
        retval |= UA_PrintContext_addNewlineTabs(&ctx, ctx.depth);
        retval |= UA_PrintContext_addString(&ctx, "Data Type Members: {");
    }
    for (UA_UInt32 i = 0; i < size; i++) {
        retval |= UA_PrintDataTypeMember(&dataType->members[i], &out);
        retval |= UA_PrintContext_addUAString(&ctx, &out);
        UA_String_clear(&out);
    }
    typePropIt_t typePropIt = dataTypeMap.find(UA_NodeId_SDBMHash(&dataType->typeId));
    if (dataType->typeKind == UA_DATATYPEKIND_STRUCTURE) {
        if (typePropIt != dataTypeMap.end()) {
            if (isOptionSet(&typePropIt->second.subTypeOfId)) {
                retval |= UA_PrintContext_addNewlineTabs(&ctx, ctx.depth);
                retval |= UA_PrintContext_addString(&ctx, "}");
                retval |= UA_PrintContext_addNewlineTabs(&ctx, ctx.depth);
                retval |= UA_PrintContext_addString(&ctx, "OptionSet Values: {");
                for (UA_UInt32 i = 0; i < typePropIt->second.structureDefinition.size(); i++) {
                    ctx.depth++;
                    retval |= UA_PrintContext_addNewlineTabs(&ctx, ctx.depth);
                    for (UA_UInt32 j = 0; j < typePropIt->second.structureDefinition[i].fieldsSize; j++) {
                        retval |= UA_PrintContext_addString(&ctx, "[0x");
                        retval |= printUInt32(&ctx, pow(2, j), 4, true);
                        retval |= UA_PrintContext_addString(&ctx, "] ");
                        retval |= UA_PrintContext_addUAString(&ctx, &typePropIt->second.structureDefinition[i].fields[j].name);
                        if (j + 1 < (UA_UInt32)typePropIt->second.structureDefinition[i].fieldsSize)
                            retval |= UA_PrintContext_addNewlineTabs(&ctx, ctx.depth);
                    }
                    ctx.depth--;
                }
            }
        }
    }
    else if (dataType->typeKind == UA_DATATYPEKIND_ENUM) {
        if (typePropIt != dataTypeMap.end()) {
            retval |= UA_PrintContext_addNewlineTabs(&ctx, ctx.depth);
            retval |= UA_PrintContext_addString(&ctx, "ENUM values: {");
            ctx.depth++;
            retval |= UA_PrintContext_addNewlineTabs(&ctx, ctx.depth);
            for (UA_UInt32 i = 0; i < typePropIt->second.enumValueSet.size(); i++) {
                retval |= UA_PrintContext_addUAString(&ctx, &typePropIt->second.enumValueSet[i].displayName.text);
                retval |= UA_PrintContext_addString(&ctx, " (");
                retval |= printUInt32(&ctx, (UA_UInt32)typePropIt->second.enumValueSet[i].value);
                retval |= UA_PrintContext_addString(&ctx, ")");
                if (i + 1 < (UA_UInt32)typePropIt->second.enumValueSet.size())
                    retval |= UA_PrintContext_addNewlineTabs(&ctx, ctx.depth);
            }
            ctx.depth--;
            retval |= UA_PrintContext_addNewlineTabs(&ctx, ctx.depth);
            retval |= UA_PrintContext_addString(&ctx, "}");
        }
    }
    if (size) {
        retval |= UA_PrintContext_addNewlineTabs(&ctx, ctx.depth);
        retval |= UA_PrintContext_addString(&ctx, "}");
    }
    ctx.depth--;
    retval |= UA_PrintContext_addNewlineTabs(&ctx, ctx.depth);
    retval |= UA_PrintContext_addString(&ctx, "}");
    retval |= UA_PrintContext_addNewlineTabs(&ctx, ctx.depth);

    /* Allocate memory for the output */
    if (retval == UA_STATUSCODE_GOOD) {
        size_t total = 0;
        UA_PrintOutput* out;
        TAILQ_FOREACH(out, &ctx.outputs, next)
            total += out->length;
        retval |= UA_ByteString_allocBuffer((UA_String*)output, total);
    }
    else {
        printf("DEBUG STOP\n");
    }
    /* Write the output */
    if (retval == UA_STATUSCODE_GOOD) {
        size_t pos = 0;
        UA_PrintOutput* out;
        TAILQ_FOREACH(out, &ctx.outputs, next) {
            memcpy(&output->data[pos], out->data, out->length);
            pos += out->length;
        }
    }
    /* Free the context */
    UA_PrintOutput* o, * o2;
    TAILQ_FOREACH_SAFE(o, &ctx.outputs, next, o2) {
        TAILQ_REMOVE(&ctx.outputs, o, next);
        UA_free(o);
    }
    return retval;
}

// prints data type member to UA_String 
UA_StatusCode UA_PrintDataTypeMember(UA_DataTypeMember* dataTypeMember, UA_String* output) {
    UA_PrintContext ctx{};
    UA_StatusCode retval;
    UA_String out;
    UA_UInt32 val;

    if (!dataTypeMember) {
        UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "UA_PrintDataTypeMember: Parameter 1 (UA_DataTypeMember*) invalid");
        return UA_STATUSCODE_BADUNEXPECTEDERROR;
    }
    if (!output) {
        UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "UA_PrintDataTypeMember: Parameter 2 (UA_String*) invalid");
        return UA_STATUSCODE_BADUNEXPECTEDERROR;
    }
    retval = UA_STATUSCODE_GOOD;
    ctx.depth = 2; // main use as a sub-function of UA_PrintDataType
    TAILQ_INIT(&ctx.outputs);
    UA_String_init(output);
    retval |= UA_PrintContext_addNewlineTabs(&ctx, ctx.depth);
#ifdef UA_ENABLE_TYPEDESCRIPTION
    UA_PrintContext_addString(&ctx, dataTypeMember->memberName);
    retval |= UA_PrintContext_addString(&ctx, ": {");
    ctx.depth++;
#endif
    retval |= UA_PrintContext_addNewlineTabs(&ctx, ctx.depth);
#ifdef UA_ENABLE_TYPEDESCRIPTION
    UA_PrintContext_addName(&ctx, "Name");
    UA_PrintContext_addString(&ctx, dataTypeMember->memberType->typeName);
#endif

    retval |= UA_PrintContext_addNewlineTabs(&ctx, ctx.depth);
    retval |= UA_PrintContext_addString(&ctx, "Is Array: ");
    retval |= UA_PrintContext_addString(&ctx, dataTypeMember->isArray ? "TRUE" : "FALSE");

    retval |= UA_PrintContext_addNewlineTabs(&ctx, ctx.depth);
    retval |= UA_PrintContext_addString(&ctx, "Is Optional: ");
    retval |= UA_PrintContext_addString(&ctx, dataTypeMember->isOptional ? "TRUE" : "FALSE");

    retval |= UA_PrintContext_addNewlineTabs(&ctx, ctx.depth);
    retval |= UA_PrintContext_addString(&ctx, "Padding Bytes: ");
    val = dataTypeMember->padding;
    UA_print(&val, &UA_TYPES[UA_TYPES_UINT16], &out);
    UA_PrintContext_addUAString(&ctx, &out);
    UA_String_clear(&out);

    ctx.depth--;
    retval |= UA_PrintContext_addNewlineTabs(&ctx, ctx.depth);
    retval |= UA_PrintContext_addString(&ctx, "}");
    /* Allocate memory for the output */
    if (retval == UA_STATUSCODE_GOOD) {
        size_t total = 0;
        UA_PrintOutput* out;
        TAILQ_FOREACH(out, &ctx.outputs, next)
            total += out->length;
        retval = UA_ByteString_allocBuffer((UA_String*)output, total);
    }
    /* Write the output */
    if (retval == UA_STATUSCODE_GOOD) {
        size_t pos = 0;
        UA_PrintOutput* out;
        TAILQ_FOREACH(out, &ctx.outputs, next) {
            memcpy(&output->data[pos], out->data, out->length);
            pos += out->length;
        }
    }
    /* Free the context */
    UA_PrintOutput* o, * o2;
    TAILQ_FOREACH_SAFE(o, &ctx.outputs, next, o2) {
        TAILQ_REMOVE(&ctx.outputs, o, next);
        UA_free(o);
    }
    return retval;
}

// prints dictionaries of the OPC UA server to UA_String
UA_StatusCode UA_PrintDictionaries(UA_Client* client, UA_String* output) {
    std::map<UA_UInt32, std::string> dictionaries;
    UA_PrintContext ctx{};
    UA_StatusCode retval;
    UA_String out;
    UA_UInt32 nameSpaceIndex = 0;

    if (!client) {
        UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "UA_PrintDictionaries: Client session invalid");
        return UA_STATUSCODE_BADUNEXPECTEDERROR;
    }
    if (!output) {
        UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "UA_PrintDictionaries: Parameter 2 (UA_String*) invalid");
        return UA_STATUSCODE_BADUNEXPECTEDERROR;
    }
    retval = getDictionaries(client, &dictionaries);
    if (retval != UA_STATUSCODE_GOOD) {
        UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "UA_PrintDictionaries: Could not retrieve OPC UA dictionary");
        return retval;
    }
    ctx.depth = 0;
    TAILQ_INIT(&ctx.outputs);
    UA_String_init(output);
    for (std::pair<const UA_UInt32, std::string>& currentDictionary : dictionaries) {
        retval |= UA_PrintContext_addString(&ctx, "namespace ");
        nameSpaceIndex = currentDictionary.first;
        UA_print(&nameSpaceIndex, &UA_TYPES[UA_TYPES_UINT32], &out);
        UA_PrintContext_addUAString(&ctx, &out);
        UA_String_clear(&out);
        retval |= UA_PrintContext_addString(&ctx, ":\n{");
        ctx.depth++;
        std::string s;
        std::istringstream f(currentDictionary.second.c_str());
        while (std::getline(f, s, '\n')) {
            out = UA_STRING_ALLOC(s.c_str());
            retval |= UA_PrintContext_addNewlineTabs(&ctx, ctx.depth);
            UA_PrintContext_addUAString(&ctx, &out);
            UA_String_clear(&out);
        }
        ctx.depth--;
        retval |= UA_PrintContext_addNewlineTabs(&ctx, ctx.depth);
        retval |= UA_PrintContext_addString(&ctx, "}");
        retval |= UA_PrintContext_addNewlineTabs(&ctx, ctx.depth);
    }
    /* Allocate memory for the output */
    if (retval == UA_STATUSCODE_GOOD) {
        size_t total = 0;
        UA_PrintOutput* out;
        TAILQ_FOREACH(out, &ctx.outputs, next)
            total += out->length;
        retval = UA_ByteString_allocBuffer((UA_String*)output, total);
    }
    /* Write the output */
    if (retval == UA_STATUSCODE_GOOD) {
        size_t pos = 0;
        UA_PrintOutput* out;
        TAILQ_FOREACH(out, &ctx.outputs, next) {
            memcpy(&output->data[pos], out->data, out->length);
            pos += out->length;
        }
    }
    /* Free the context */
    UA_PrintOutput* o, * o2;
    TAILQ_FOREACH_SAFE(o, &ctx.outputs, next, o2) {
        TAILQ_REMOVE(&ctx.outputs, o, next);
        UA_free(o);
    }
    return retval;
}

// prints variant data type ENUM to UA_String
UA_StatusCode UA_PrintEnum(const UA_Variant* pData, customTypeProperties_t* customTypeProperties, UA_String* output) {
    UA_Int32 value;
    UA_PrintContext ctx;
    UA_StatusCode retval;

    if (!pData) {
        UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "UA_PrintEnum: Parameter 1 (UA_Variant*) invalid");
        return UA_STATUSCODE_BADUNEXPECTEDERROR;
    }
    if (!customTypeProperties || !customTypeProperties->enumValueSet.size() || pData->type->membersSize) {
        UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "UA_PrintEnum: Parameter 2 (customTypeProperties_t*) invalid");
        return UA_STATUSCODE_BADUNEXPECTEDERROR;
    }
    if (!output) {
        UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "UA_PrintEnum: Parameter 3 (UA_String*) invalid");
        return UA_STATUSCODE_BADUNEXPECTEDERROR;
    }
    retval = UA_STATUSCODE_GOOD;
    ctx = UA_PrintContext();
    ctx.depth = 0;
    TAILQ_INIT(&ctx.outputs);
    UA_String_init(output);
    value = *(UA_Int32*)pData->data;
    retval |= UA_PrintContext_addString(&ctx, "{");
    ctx.depth++;
#ifdef UA_ENABLE_TYPEDESCRIPTION
    retval |= UA_PrintContext_addNewlineTabs(&ctx, ctx.depth);
    retval |= UA_PrintContext_addName(&ctx, "DataType");
    retval |= UA_PrintContext_addString(&ctx, customTypeProperties->dataType.typeName);
    retval |= UA_PrintContext_addString(&ctx, ",");
#endif
    retval |= UA_PrintContext_addNewlineTabs(&ctx, ctx.depth);
    retval |= UA_PrintContext_addName(&ctx, "Value");
    UA_UInt32 i = 0;
    for (; i < customTypeProperties->enumValueSet.size(); i++) {
        if (customTypeProperties->enumValueSet.at(i).value == value) {
            retval |= UA_PrintContext_addUAString(&ctx, &customTypeProperties->enumValueSet.at(i).displayName.text);
            retval |= UA_PrintContext_addString(&ctx, " (");
            retval |= printUInt32(&ctx, value);
            retval |= UA_PrintContext_addString(&ctx, ")");
            break;
        }
    }
    if (i >= customTypeProperties->enumValueSet.size()) {
        retval |= printUInt32(&ctx, value);
    }
    ctx.depth--;
    retval |= UA_PrintContext_addNewlineTabs(&ctx, ctx.depth);
    retval |= UA_PrintContext_addString(&ctx, "}");
    /* Allocate memory for the output */
    if (retval == UA_STATUSCODE_GOOD) {
        size_t total = 0;
        UA_PrintOutput* out;
        TAILQ_FOREACH(out, &ctx.outputs, next)
            total += out->length;
        retval = UA_ByteString_allocBuffer((UA_String*)output, total);
    }
    /* Write the output */
    if (retval == UA_STATUSCODE_GOOD) {
        size_t pos = 0;
        UA_PrintOutput* out;
        TAILQ_FOREACH(out, &ctx.outputs, next) {
            memcpy(&output->data[pos], out->data, out->length);
            pos += out->length;
        }
    }
    /* Free the context */
    UA_PrintOutput* o, * o2;
    TAILQ_FOREACH_SAFE(o, &ctx.outputs, next, o2) {
        TAILQ_REMOVE(&ctx.outputs, o, next);
        UA_free(o);
    }
    return retval;
}

// prints variant data type STRUCTURE to UA_String
UA_StatusCode UA_PrintStructure(const UA_Variant* data, UA_String* output) {
    const UA_DataType* dataType;
    UA_PrintContext ctx;
    UA_PrintOutput* out;
    UA_StatusCode retval;
    UA_String outString;
    uintptr_t ptrs;

    if (!data) {
        UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "UA_PrintStructure: Parameter 1 (UA_Variant*) invalid");
        return UA_STATUSCODE_BADUNEXPECTEDERROR;
    }
    if (!output) {
        UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "UA_PrintStructure: Parameter 2 (UA_String*) invalid");
        return UA_STATUSCODE_BADUNEXPECTEDERROR;
    }
    retval = UA_STATUSCODE_GOOD;
    ctx = UA_PrintContext();
    ctx.depth = 0;
    dataType = data->type;
    ptrs = (uintptr_t)data->data;
    TAILQ_INIT(&ctx.outputs);
    UA_String_init(output);
    if (!dataType) {
        *output = UA_STRING_ALLOC("NullVariant");
        return UA_STATUSCODE_GOOD;
    }
    retval |= UA_PrintContext_addString(&ctx, "{");
    ctx.depth++;
#ifdef UA_ENABLE_TYPEDESCRIPTION
    retval |= UA_PrintContext_addNewlineTabs(&ctx, ctx.depth);
    retval |= UA_PrintContext_addName(&ctx, "DataType");
    retval |= UA_PrintContext_addString(&ctx, dataType->typeName);
    retval |= UA_PrintContext_addString(&ctx, ",");
#endif
    retval |= UA_PrintContext_addNewlineTabs(&ctx, ctx.depth);
    retval |= UA_PrintContext_addName(&ctx, "Value");
    if (UA_Variant_isScalar(data)) {
        retval |= UA_PrintContext_addString(&ctx, "{");
        ctx.depth++;
        retval |= UA_PrintContext_addNewlineTabs(&ctx, ctx.depth);
        // print OptionSet
        if (dataType->membersSize == 2 && 
            UA_NodeId_equal(&dataType->members[0].memberType->typeId, &NS0ID_BYTESTRING) && 
            UA_NodeId_equal(&dataType->members[1].memberType->typeId, &NS0ID_BYTESTRING)) {
            typePropIt_t typePropIt = dataTypeMap.find(UA_NodeId_SDBMHash(&dataType->typeId));
            if (typePropIt != dataTypeMap.end()) {
                UA_Byte* pValue;
                UA_Byte* pValidBits;
                UA_Byte resultingBits;
                ptrs += dataType->members[0].padding;
                pValue = ((UA_ByteString*)ptrs)->data;
                ptrs += UA_TYPES[UA_TYPES_BYTESTRING].memSize;
                ptrs += dataType->members[1].padding;
                pValidBits = ((UA_ByteString*)ptrs)->data;
                resultingBits = *pValue & *pValidBits;
                for (UA_UInt32 i = 0; i < typePropIt->second.structureDefinition.size(); i++) {
                    for (UA_UInt32 j = 0; j < typePropIt->second.structureDefinition[i].fieldsSize; j++) {
                        retval |= UA_PrintContext_addUAString(&ctx, &typePropIt->second.structureDefinition[i].fields[j].name);
                        retval |= UA_PrintContext_addString(&ctx, ": ");
                        retval |= UA_PrintContext_addString(&ctx, resultingBits & 0x01 << j ? "TRUE" : "FALSE");
                        if (j < typePropIt->second.structureDefinition[i].fieldsSize - 1)
                            retval |= UA_PrintContext_addNewlineTabs(&ctx, ctx.depth);
                    }
                }
            }
        } // print structure members
        else {
            for (UA_UInt32 i = 0; i < dataType->membersSize; i++) {
                typePropIt_t typePropIt = dataTypeMap.end();
                UA_DataTypeMember* dataTypeMember = &dataType->members[i];
#ifdef UA_ENABLE_TYPEDESCRIPTION
                retval |= UA_PrintContext_addName(&ctx, dataTypeMember->memberName);                
#endif
                ptrs += dataTypeMember->padding;
                if (dataTypeMember->isOptional) {
                    if (*(UA_Int32**)ptrs) {
                        if (dataTypeMember->isArray) {
                            const size_t size = *((const size_t*)ptrs);
                            ptrs += sizeof(size_t);
                            retval |= printArray(&ctx, *(void* const*)ptrs, size, dataTypeMember->memberType);
                            ptrs += sizeof(void*);
                        }
                        else {
                            retval |= UA_print(*(UA_Int32**)ptrs, dataTypeMember->memberType, &outString);
                            out = UA_PrintContext_addOutput(&ctx, outString.length);
                            if (!out)
                                retval |= UA_STATUSCODE_BADOUTOFMEMORY;
                            else
                                memcpy(&out->data, outString.data, outString.length);
                            UA_String_clear(&outString);
                        }
                    }
                    else {
                        retval |= UA_PrintContext_addString(&ctx, "(disabled)");
                    }
                    ptrs += sizeof(void*);
                    if (i < dataType->membersSize - 1u)
                        retval |= UA_PrintContext_addNewlineTabs(&ctx, ctx.depth);
                }
                else {
                    if (dataTypeMember->isArray) {
                        const size_t size = *((const size_t*)ptrs);
                        ptrs += sizeof(size_t);
                        retval |= printArray(&ctx, *(void* const*)ptrs, size, dataTypeMember->memberType);
                        ptrs += sizeof(void*);
                    }
                    else {
                        retval |= UA_print((const void*)ptrs, dataTypeMember->memberType, &outString);
                        if(dataTypeMember->memberType->typeKind != UA_DATATYPEKIND_ENUM) {
                            out = UA_PrintContext_addOutput(&ctx, outString.length);
                            if (!out)
                                retval |= UA_STATUSCODE_BADOUTOFMEMORY;
                            else
                                memcpy(&out->data, outString.data, outString.length);
                        }
                        else {
                            char* pError;
                            std::string ancestorsNameValue = std::string((char*)outString.data, outString.length);                            
                            UA_UInt32 i = (UA_UInt32)strtoll(ancestorsNameValue.c_str(), &pError, 10);
                            typePropIt = dataTypeMap.find(UA_NodeId_SDBMHash(&dataTypeMember->memberType->typeId));
                            if (ancestorsNameValue.c_str() != pError && typePropIt != dataTypeMap.end()) {
                                for (UA_UInt32 j = 0; j < typePropIt->second.enumValueSet.size(); j++) {
                                    if (i == typePropIt->second.enumValueSet.at(j).value) {
                                        retval |= UA_PrintContext_addUAString(&ctx, &typePropIt->second.enumValueSet.at(j).displayName.text);
                                        break;
                                    }
                                }
                            }
                        }
                        UA_String_clear(&outString);
                        ptrs += dataTypeMember->memberType->memSize;
                    }
                    if (i < dataType->membersSize - 1u)
                        retval |= UA_PrintContext_addNewlineTabs(&ctx, ctx.depth);
                }
            }
        }
    }
    ctx.depth--;
    retval |= UA_PrintContext_addNewlineTabs(&ctx, ctx.depth);
    retval |= UA_PrintContext_addString(&ctx, "}");
    ctx.depth--;
    retval |= UA_PrintContext_addNewlineTabs(&ctx, ctx.depth);
    retval |= UA_PrintContext_addString(&ctx, "}");
    /* Allocate memory for the output */
    if (retval == UA_STATUSCODE_GOOD) {
        size_t total = 0;
        UA_PrintOutput* out;
        TAILQ_FOREACH(out, &ctx.outputs, next)
            total += out->length;
        retval = UA_ByteString_allocBuffer((UA_String*)output, total);
    }
    /* Write the output */
    if (retval == UA_STATUSCODE_GOOD) {
        size_t pos = 0;
        UA_PrintOutput* out;
        TAILQ_FOREACH(out, &ctx.outputs, next) {
            memcpy(&output->data[pos], out->data, out->length);
            pos += out->length;
        }
    }
    /* Free the context */
    UA_PrintOutput* o, * o2;
    TAILQ_FOREACH_SAFE(o, &ctx.outputs, next, o2) {
        TAILQ_REMOVE(&ctx.outputs, o, next);
        UA_free(o);
    }
    return retval;
}

// prints data type kind name to string
void UA_PrintTypeKind(UA_UInt32 typeKind, UA_String* output) {
    if (!output) {
        UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "UA_PrintTypeKind: Parameter 2 (UA_String*) invalid");
        return;
    }
    switch (typeKind) {
    case UA_DATATYPEKIND_BOOLEAN: {
        *output = UA_STRING_ALLOC("boolean");
        break;
    }
    case UA_DATATYPEKIND_SBYTE: {
        *output = UA_STRING_ALLOC("signed byte");
        break;
    }
    case UA_DATATYPEKIND_BYTE: {
        *output = UA_STRING_ALLOC("unsigned byte");
        break;
    }
    case UA_DATATYPEKIND_INT16: {
        *output = UA_STRING_ALLOC("signed integer (16 bit)");
        break;
    }
    case UA_DATATYPEKIND_UINT16: {
        *output = UA_STRING_ALLOC("unsigned integer (16 bit)");
        break;
    }
    case UA_DATATYPEKIND_INT32: {
        *output = UA_STRING_ALLOC("signed integer (32 bit)");
        break;
    }
    case UA_DATATYPEKIND_UINT32: {
        *output = UA_STRING_ALLOC("unsigned integer (32 bit)");
        break;
    }
    case UA_DATATYPEKIND_INT64: {
        *output = UA_STRING_ALLOC("signed integer (64 bit)");
        break;
    }
    case UA_DATATYPEKIND_UINT64: {
        *output = UA_STRING_ALLOC("unsigned integer (64 bit)");
        break;
    }
    case UA_DATATYPEKIND_FLOAT: {
        *output = UA_STRING_ALLOC("float");
        break;
    }
    case UA_DATATYPEKIND_DOUBLE: {
        *output = UA_STRING_ALLOC("double");
        break;
    }
    case UA_DATATYPEKIND_STRING: {
        *output = UA_STRING_ALLOC("string");
        break;
    }
    case UA_DATATYPEKIND_DATETIME: {
        *output = UA_STRING_ALLOC("date time");
        break;
    }
    case UA_DATATYPEKIND_GUID: {
        *output = UA_STRING_ALLOC("GUID");
        break;
    }
    case UA_DATATYPEKIND_BYTESTRING: {
        *output = UA_STRING_ALLOC("byte string");
        break;
    }
    case UA_DATATYPEKIND_XMLELEMENT: {
        *output = UA_STRING_ALLOC("xml string");
        break;
    }
    case UA_DATATYPEKIND_NODEID: {
        *output = UA_STRING_ALLOC("node ID");
        break;
    }
    case UA_DATATYPEKIND_EXPANDEDNODEID: {
        *output = UA_STRING_ALLOC("expanded node ID");
        break;
    }
    case UA_DATATYPEKIND_STATUSCODE: {
        *output = UA_STRING_ALLOC("status code");
        break;
    }
    case UA_DATATYPEKIND_QUALIFIEDNAME: {
        *output = UA_STRING_ALLOC("qualified name");
        break;
    }
    case UA_DATATYPEKIND_LOCALIZEDTEXT: {
        *output = UA_STRING_ALLOC("localized text");
        break;
    }
    case UA_DATATYPEKIND_EXTENSIONOBJECT: {
        *output = UA_STRING_ALLOC("extension object");
        break;
    }
    case UA_DATATYPEKIND_DATAVALUE: {
        *output = UA_STRING_ALLOC("data value");
        break;
    }
    case UA_DATATYPEKIND_VARIANT: {
        *output = UA_STRING_ALLOC("variant");
        break;
    }
    case UA_DATATYPEKIND_DIAGNOSTICINFO: {
        *output = UA_STRING_ALLOC("diagnostic info");
        break;
    }
    case UA_DATATYPEKIND_DECIMAL: {
        *output = UA_STRING_ALLOC("decimal");
        break;
    }
    case UA_DATATYPEKIND_ENUM: {
        *output = UA_STRING_ALLOC("enumeration");
        break;
    }
    case UA_DATATYPEKIND_STRUCTURE: {
        *output = UA_STRING_ALLOC("structure");
        break;
    }
    case  UA_DATATYPEKIND_OPTSTRUCT/* struct with optional fields */: {
        *output = UA_STRING_ALLOC("structure with optional fields");
        break;
    }
    case UA_DATATYPEKIND_UNION: {
        *output = UA_STRING_ALLOC("union");
        break;
    }
    case UA_DATATYPEKIND_BITFIELDCLUSTER /* bitfields + padding */: {
        *output = UA_STRING_ALLOC("bitfields + padding");
        break;
    }
    default: {
        *output = UA_STRING_ALLOC("unknown");
    }
    }
}

// prints variant data type UNION to UA_String
UA_StatusCode UA_PrintUnion(const UA_Variant* data, UA_String* output) {
    const UA_DataType* dataType;
    UA_PrintContext ctx;
    UA_StatusCode retval;
    UA_String outString;
    uintptr_t ptrs;

    if (!data) {
        UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "UA_PrintUnion: Parameter 1 (UA_Variant*) invalid");
        return UA_STATUSCODE_BADUNEXPECTEDERROR;
    }
    if (!output) {
        UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "UA_PrintUnion: Parameter 2 (UA_String*) invalid");
        return UA_STATUSCODE_BADUNEXPECTEDERROR;
    }
    retval = UA_STATUSCODE_GOOD;
    ctx = UA_PrintContext();
    ctx.depth = 0;
    dataType = data->type;
    ptrs = (uintptr_t)data->data;
    TAILQ_INIT(&ctx.outputs);
    UA_String_init(output);
    if (!dataType) {
        *output = UA_STRING_ALLOC("NullVariant");
        return UA_STATUSCODE_GOOD;
    }
    retval |= UA_PrintContext_addString(&ctx, "{");
    ctx.depth++;

#ifdef UA_ENABLE_TYPEDESCRIPTION
    retval |= UA_PrintContext_addNewlineTabs(&ctx, ctx.depth);
    retval |= UA_PrintContext_addName(&ctx, "DataType");
    retval |= UA_PrintContext_addString(&ctx, dataType->typeName);
    retval |= UA_PrintContext_addString(&ctx, ",");
#endif
    retval |= UA_PrintContext_addNewlineTabs(&ctx, ctx.depth);
    if (UA_Variant_isScalar(data)) {
        retval |= UA_PrintContext_addName(&ctx, "SwitchValue");
        UA_UInt32 switchIndex = *((UA_UInt32*)ptrs);
        retval |= printUInt32(&ctx, switchIndex);
        //ptrs += UA_TYPES[dataType->members[0].memberTypeIndex].memSize;
        ptrs += dataType->members[1].padding;
        if (switchIndex > 0 && switchIndex <= dataType->membersSize) {
            const UA_DataTypeMember* unionDataTypeMember = &dataType->members[switchIndex - 1];
            if (unionDataTypeMember) {
                retval |= UA_PrintContext_addNewlineTabs(&ctx, ctx.depth);
#ifdef UA_ENABLE_TYPEDESCRIPTION
                retval |= UA_PrintContext_addName(&ctx, "Name");
                retval |= UA_PrintContext_addString(&ctx, unionDataTypeMember->memberName);
                retval |= UA_PrintContext_addNewlineTabs(&ctx, ctx.depth);
#endif
                retval |= UA_PrintContext_addName(&ctx, "Value");
                UA_print((void*)ptrs, unionDataTypeMember->memberType, &outString);
                if (retval == UA_STATUSCODE_GOOD) {
                    retval |= UA_PrintContext_addUAString(&ctx, &outString);
                    UA_String_clear(&outString);
                }
                else {
                    retval = UA_PrintContext_addString(&ctx, UA_StatusCode_name(retval));
                }
            }
            else {
                retval |= UA_PrintContext_addString(&ctx, "UNION data type unknown");
            }
        }
        else {
            retval |= UA_PrintContext_addNewlineTabs(&ctx, ctx.depth);
            retval |= UA_PrintContext_addName(&ctx, "Value");
            retval |= UA_PrintContext_addString(&ctx, "(disabled)");
        }
    }
    ctx.depth--;
    retval |= UA_PrintContext_addNewlineTabs(&ctx, ctx.depth);
    retval |= UA_PrintContext_addString(&ctx, "}");
    /* Allocate memory for the output */
    if (retval == UA_STATUSCODE_GOOD) {
        size_t total = 0;
        UA_PrintOutput* out;
        TAILQ_FOREACH(out, &ctx.outputs, next)
            total += out->length;
        retval = UA_ByteString_allocBuffer((UA_String*)output, total);
    }
    /* Write the output */
    if (retval == UA_STATUSCODE_GOOD) {
        size_t pos = 0;
        UA_PrintOutput* out;
        TAILQ_FOREACH(out, &ctx.outputs, next) {
            memcpy(&output->data[pos], out->data, out->length);
            pos += out->length;
        }
    }
    /* Free the context */
    UA_PrintOutput* o, * o2;
    TAILQ_FOREACH_SAFE(o, &ctx.outputs, next, o2) {
        TAILQ_REMOVE(&ctx.outputs, o, next);
        UA_free(o);
    }
    return retval;
}

// prints values of custom and base data types to UA_String
UA_StatusCode UA_PrintValue(UA_Client* client, UA_NodeId nodeId, UA_Variant* data, UA_String* output) {
    UA_StatusCode retval;
    if (!client) {
        UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "UA_printValue: Client session invalid");
        return UA_STATUSCODE_BADUNEXPECTEDERROR;
    }
    if (UA_NodeId_isNull(&nodeId)) {
        UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "UA_printValue: Parameter 2 (UA_NodeId) invalid");
        return UA_STATUSCODE_BADUNEXPECTEDERROR;
    }
    if (!data) {
        UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "UA_printValue: Parameter 3 (UA_Variant*) invalid");
        return UA_STATUSCODE_BADUNEXPECTEDERROR;
    }
    if (!output) {
        UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "UA_printValue: Parameter 4 (UA_String*) invalid");
        return UA_STATUSCODE_BADUNEXPECTEDERROR;
    }
    retval = UA_STATUSCODE_GOOD;
    // STRUCTURE / STRUCTURE WITH OPTINAL FIELDS / UNION / OPTION SET
    if (data->type->typeKind == UA_DATATYPEKIND_STRUCTURE || data->type->typeKind == UA_DATATYPEKIND_OPTSTRUCT)
        retval = UA_PrintStructure(data, output);
    // ENUM
    else if (UA_NodeId_equal(&data->type->typeId, &NS0ID_INT32) && !data->type->membersSize && !data->arrayLength) {
        UA_NodeId typeId;
        typePropIt_t typePropIt;
        retval = UA_Client_readDataTypeAttribute(client, nodeId, &typeId);
        if (retval == UA_STATUSCODE_GOOD) {
            typePropIt = dataTypeMap.find(UA_NodeId_SDBMHash(&typeId));
            if (typePropIt != dataTypeMap.end() && typePropIt->second.enumValueSet.size())
                retval = UA_PrintEnum(data, &typePropIt->second, output);
            else
                retval = UA_print(data, &UA_TYPES[UA_TYPES_VARIANT], output);
        }
        else
            retval = UA_print(data, &UA_TYPES[UA_TYPES_VARIANT], output);
    }
    // UNION
    else if (data->type->typeKind == UA_DATATYPEKIND_UNION)
        retval = UA_PrintUnion(data, output);
    // FALLBACK
    else
        retval = UA_print(data, &UA_TYPES[UA_TYPES_VARIANT], output);
    return retval;
}

// retrieve custom data types from the server node /Types/DataTypes/BaseDataType
// results are stored in dataTypeMap and dataTypeNameMap
UA_StatusCode scan4BaseDataTypes(UA_Client* client) {
    std::vector<UA_NodeId> ids; // nodes to visit
    std::vector<UA_NodeId> dataTypeIds; // data type nodes
    std::vector<UA_NodeId> customDataTypeIds; // custom data type nodes
    std::string csBrowseName;
    UA_BrowseResponse bResp;
    UA_NodeClass nodeClass;
    UA_QualifiedName browseName;
    UA_StatusCode retval;
    UA_UInt32 typeIdHash;
    UA_String out;

    if (!client) {
        UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "scan4BaseDataTypes: Client session invalid");
        return UA_STATUSCODE_BADUNEXPECTEDERROR;
    }
    UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "scan4BaseDataTypes: scanning for custom data types in progress ...");
    retval = UA_STATUSCODE_GOOD;
    // start scanning at server node /Types/DataTypes/BaseDataType
    ids.push_back(NS0ID_BASEDATATYPE);
    // collect custom data type node IDs
    do {
        for (const UA_NodeId& id : ids) {
            retval |= browseNodeId(client, id, &bResp);
            if (retval == UA_STATUSCODE_GOOD)
                scanForTypeIds(&bResp, &dataTypeIds, &customDataTypeIds);
        }
        ids.clear();
        ids.swap(dataTypeIds);
        if(ids.size())
            UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "scan4BaseDataTypes: just scanned branch has %d IDs ...", (UA_UInt32)ids.size());
    } while (!ids.empty());
    UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "scan4BaseDataTypes: %d custom node IDs are now processed ...", (UA_UInt32)customDataTypeIds.size());
    // process custom data type node IDs
    for (const UA_NodeId& id : customDataTypeIds) {
        typeIdHash = UA_NodeId_SDBMHash(&id);
        retval = UA_Client_readNodeClassAttribute(client, id, &nodeClass);
        if (retval != UA_STATUSCODE_GOOD) {
            UA_print(&id, &UA_TYPES[UA_TYPES_NODEID], &out);
            UA_LOG_WARNING(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "scan4BaseDataTypes: Could not read \"NodeClassAttribute\" for %.*s. (%s)", (UA_UInt16)out.length, out.data, UA_StatusCode_name(retval));
            UA_String_clear(&out);
            continue;
        }
        // retrieve node id references
        retval |= browseNodeId(client, id, &bResp);
        if (retval != UA_STATUSCODE_GOOD) {
            UA_print(&id, &UA_TYPES[UA_TYPES_NODEID], &out);
            UA_LOG_WARNING(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "scan4BaseDataTypes: Could not browse %.*s. (%s)", (UA_UInt16)out.length, out.data, UA_StatusCode_name(retval));
            UA_String_clear(&out);
            continue;
        }
        // the link between node tree and dictionary is the BrowseName
        retval = UA_Client_readBrowseNameAttribute(client, id, &browseName);
        if (retval != UA_STATUSCODE_GOOD) {
            UA_print(&id, &UA_TYPES[UA_TYPES_NODEID], &out);
            UA_LOG_WARNING(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "scan4BaseDataTypes: Could not read \"BrowseNameAttribute\" for %.*s. (%s)", (UA_UInt16)out.length, out.data, UA_StatusCode_name(retval));
            UA_String_clear(&out);
            continue;
        }
        customTypeProperties_t customTypeProperties;
        customTypePropertiesInit(&customTypeProperties, &id);
        csBrowseName = std::string((char*)browseName.name.data, browseName.name.length);
#ifdef UA_ENABLE_TYPEDESCRIPTION
        customTypeProperties.dataType.typeName = (char*)UA_malloc(csBrowseName.length() * sizeof(char) + 1);
        if (customTypeProperties.dataType.typeName)
            strcpy((char*)customTypeProperties.dataType.typeName, csBrowseName.c_str());
#endif
        for (UA_UInt16 i = 0; i < bResp.resultsSize; i++) {
            // check data type reference first and save type in customTypeProperties
            for (UA_UInt16 j = 0; j < bResp.results[i].referencesSize; j++) {
                if (!UA_NodeId_equal(&bResp.results[i].references[j].referenceTypeId, &NS0ID_HASSUBTYPE))
                    continue;
                UA_NodeId_copy(&bResp.results[i].references[j].nodeId.nodeId, &customTypeProperties.subTypeOfId);
                getSubTypeProperties(&customTypeProperties.subTypeOfId, &customTypeProperties);
            } // end for(UA_UInt16 j = 0; j < bResp.results[i].referencesSize; j++)
            // check other references
            for (UA_UInt16 j = 0; j < bResp.results[i].referencesSize; j++) {
                // check for binary encoding node ID
                if (UA_NodeId_equal(&bResp.results[i].references[j].referenceTypeId, &NS0ID_HASENCODING))
                    UA_NodeId_copy(&bResp.results[i].references[j].nodeId.nodeId, &customTypeProperties.dataType.binaryEncodingId);
                // referenced properties check
                else if (UA_NodeId_equal(&bResp.results[i].references[j].referenceTypeId, &NS0ID_HASPROPERTY)) {
                    UA_Variant outValue;
                    retval = UA_Client_readValueAttribute(client, bResp.results[i].references[j].nodeId.nodeId, &outValue);
                    // collect structure and enumeration properties of custom data type
                    if (retval == UA_STATUSCODE_GOOD && !UA_Variant_isScalar(&outValue)) {
                        if (outValue.type->typeId.identifier.numeric == UA_NS0ID_LOCALIZEDTEXT) {
                            UA_LocalizedText* data = (UA_LocalizedText*)outValue.data;
                            UA_StructureDefinition structureDef;
                            UA_StructureDefinition_init(&structureDef);
                            structureDef.fieldsSize = outValue.arrayLength;
                            structureDef.structureType = UA_STRUCTURETYPE_STRUCTURE;
                            UA_NodeId_copy(&NS0ID_OPTIONSET, &structureDef.baseDataType);
                            if (customTypeProperties.dataType.typeKind != UA_DATATYPEKIND_ENUM)
                                structureDef.fields = (UA_StructureField*)malloc(structureDef.fieldsSize * sizeof(UA_StructureField));
                            for (UA_UInt32 i = 0; i < (UA_UInt32)outValue.arrayLength; i++) {
                                if (customTypeProperties.dataType.typeKind == UA_DATATYPEKIND_ENUM) {
                                    UA_EnumValueType enumValue;
                                    UA_EnumValueType_init(&enumValue);
                                    enumValue.value = i;
                                    UA_LocalizedText_copy(&data[i], &enumValue.description);
                                    UA_LocalizedText_copy(&data[i], &enumValue.displayName);
                                    customTypeProperties.enumValueSet.push_back(enumValue);
                                }
                                else {
                                    UA_StructureField_init(&structureDef.fields[i]);
                                    UA_LocalizedText_copy(&data[i], &structureDef.fields[i].description);
                                    UA_String_copy(&data[i].text, &structureDef.fields[i].name);
                                    structureDef.fields[i].valueRank = i;
                                    UA_NodeId_copy(&outValue.type->typeId, &structureDef.fields[i].dataType);
                                }
                            }
                            if (customTypeProperties.dataType.typeKind != UA_DATATYPEKIND_ENUM)
                                customTypeProperties.structureDefinition.push_back(structureDef);
                        } // end if(outValue.type->typeId.identifier.numeric == UA_NS0ID_LOCALIZEDTEXT)
                        else if (outValue.type->typeKind == UA_DATATYPEKIND_EXTENSIONOBJECT) {
                            if (!UA_Variant_isScalar(&outValue)) {
                                for (UA_UInt32 i = 0; i < (UA_UInt32)outValue.arrayLength; i++) {
                                    if (((UA_ExtensionObject*)outValue.data)[i].encoding == UA_EXTENSIONOBJECT_DECODED) {
                                        const UA_DataType* dataType = ((UA_ExtensionObject*)outValue.data)[i].content.decoded.type;
                                        UA_ExtensionObject* extObj = &((UA_ExtensionObject*)outValue.data)[i];
                                        if (extObj->encoding == UA_EXTENSIONOBJECT_DECODED && dataType->typeKind == UA_DATATYPEKIND_STRUCTURE) {// && dataType->typeIndex == UA_TYPES_ENUMVALUETYPE) {
                                            UA_EnumValueType* enumValue = (UA_EnumValueType*)extObj->content.decoded.data;
                                            UA_EnumValueType newEnumValue;
                                            UA_EnumValueType_copy(enumValue, &newEnumValue);
                                            customTypeProperties.enumValueSet.push_back(newEnumValue);
                                        }
                                    }
                                }
                            } // if(!UA_Variant_isScalar(&outValue))
                        } // end else if(outValue.type->typeKind == UA_DATATYPEKIND_EXTENSIONOBJECT)
                        UA_Variant_clear(&outValue);
                    } // end if(retval == UA_STATUSCODE_GOOD && !UA_Variant_isScalar(&outValue))
                } // end else if(UA_NodeId_equal(&bResp.results[i].references[j].referenceTypeId, &NS0ID_HASPROPERTY))
            } // end for(UA_UInt16 j = 0; j < bResp.results[i].referencesSize; j++)
        } // end for(UA_UInt16 i = 0; i < bResp.resultsSize; i++)
        // save custom data type and context properties to global variables dataTypeMap and dataTypeNameMap
        if (dataTypeMap.find(typeIdHash) != dataTypeMap.end()) {
            UA_LOG_WARNING(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "scan4BaseDataTypes: %.*s has a duplicate node ID", (UA_UInt16)browseName.name.length, browseName.name.data);
        }
        else {
            dataTypeMap.insert(std::pair<UA_UInt32, customTypeProperties_t>(typeIdHash, customTypeProperties));
            dataTypeNameMap.insert(std::pair<std::string, customTypeProperties_t*>(csBrowseName, &dataTypeMap[typeIdHash]));
        }
    }
    UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "scan4BaseDataTypes: finished");
    return retval;
}

// search data type tree recursively and collect node IDs
void scanForTypeIds(UA_BrowseResponse* bResp, std::vector<UA_NodeId>* dataTypeIds, std::vector<UA_NodeId>* customDataTypeIds) {
    UA_BrowseResult bRes;
    if (!bResp || !dataTypeIds || !customDataTypeIds)
        return;
    for (size_t i = 0; i < bResp->resultsSize; ++i) {
        bRes = bResp->results[i];
        for (size_t j = 0; j < bRes.referencesSize; ++j) {
            // process only forwarded references and node classes of the "Datatype" type
            if (!bResp->results[i].references[j].isForward || bResp->results[i].references[j].nodeClass != UA_NODECLASS_DATATYPE)
                continue;
            dataTypeIds->push_back(bRes.references[j].nodeId.nodeId);
            // skip base data types with NameSpaceIndex = 0
            if (bRes.references[j].nodeId.nodeId.namespaceIndex)
                customDataTypeIds->push_back(bRes.references[j].nodeId.nodeId);
        }
    }
}

UA_StatusCode scan4Variables(UA_Client* client, UA_NodeId parentNode, std::vector<UA_NodeId>* variableIds) {
    UA_StatusCode retval;
    std::vector<UA_NodeId> ids; // nodes to visit
    std::vector<UA_NodeId> subIds; // children IDs
    UA_BrowseResponse bResp;
    UA_BrowseResult bRes;
    UA_Variant value;

    if (!client) {
        UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "scan4Variables: Client session invalid");
        return UA_STATUSCODE_BADUNEXPECTEDERROR;
    }
    if (UA_NodeId_isNull(&parentNode)) {
        UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "scan4Variables: Parameter 2 (UA_NodeId) invalid");
        return UA_STATUSCODE_BADUNEXPECTEDERROR;
    }
    if (!variableIds) {
        UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "scan4Variables: Parameter 3 (std::vector<UA_NodeId>*) invalid");
        return UA_STATUSCODE_BADUNEXPECTEDERROR;
    }
    retval = UA_STATUSCODE_GOOD;
    if (UA_Client_readValueAttribute(client, parentNode, &value) == UA_STATUSCODE_GOOD)
        variableIds->push_back(parentNode);
    ids.push_back(parentNode);
    while (!ids.empty()) {
        for (const UA_NodeId& id : ids) {
            retval |= browseNodeId(client, id, &bResp);
            if (retval == UA_STATUSCODE_GOOD) {
                for (size_t i = 0; i < bResp.resultsSize; ++i) {
                    bRes = bResp.results[i];
                    for (size_t j = 0; j < bRes.referencesSize; ++j) {
                        if (bResp.results[i].references[j].nodeClass == UA_NODECLASS_VARIABLE)
                            variableIds->push_back(bResp.results[i].references[j].nodeId.nodeId);
                        else if(bResp.results[i].references[j].isForward && bResp.results[i].references[j].nodeClass == UA_NODECLASS_OBJECT)
                            subIds.push_back(bResp.results[i].references[j].nodeId.nodeId);
                    }
                }
            }
        }
        ids.swap(subIds);
        subIds.clear();
    }
    return retval;
}

int main(int argc, char* argv[]) {
    /*
    // local Unified Automation Demo server
    // https://unified-automation.com
    const char* uaUrl = "opc.tcp://localhost:48010";
    const char* parentId = "ns=2;s=Demo.Static"; //"ns=2;s=Demo.Static.Scalar.Int32";
    *//*
    // Eclipse Milo server
    // https://github.com/eclipse/milo
    const char* uaUrl = "opc.tcp://milo.digitalpetri.com:62541/milo";
    const char* parentId = "ns=2;s=CTT/Static/All Profiles";
    */
    // node-opcua server
    // https://github.com/node-opcua/node-opcua
    const char* uaUrl = "opc.tcp://opcuademo.sterfive.com:26543";
    const char* parentId = "ns=8;i=1001"; // /Simulation/Static
    

    UA_Client* client;
    UA_NodeId nodeId;
    UA_StatusCode retval;
    UA_String out;
    UA_Variant outValue;
    std::vector<UA_NodeId> variableIds;

    if (argc > 1) uaUrl = argv[1];
    if (argc > 2) parentId = argv[2];

    // open OPC UA session
    client = UA_Client_new();
    UA_ClientConfig_setDefault(UA_Client_getConfig(client));    
    retval = UA_Client_connect(client, uaUrl);
    if (retval != UA_STATUSCODE_GOOD) {
        UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "Could not open OPC UA client session to %s. (%s)", uaUrl, UA_StatusCode_name(retval));
        UA_Client_delete(client);
        return EXIT_FAILURE;
    }

    /*
    // print dictionaries of the OPC UA server
    retval = UA_PrintDictionaries(client, &out);
    if (retval == UA_STATUSCODE_GOOD) {
        printf("%.*s\n", (UA_UInt16)out.length, out.data);
        fflush(stdout);
    }
    UA_String_clear(&out);
    */

    // initialization of the custom data type
    retval = initializeCustomDataTypes(client);
    if (retval != UA_STATUSCODE_GOOD)
        UA_LOG_WARNING(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "Could not initialize custom data types. (%s)", UA_StatusCode_name(retval));

    /*
    // print custom data type map
    retval = UA_PrintCustomDataTypeMap(&out);
    if (retval == UA_STATUSCODE_GOOD) {
        printf("%.*s\n", (UA_UInt16)out.length, out.data);
        UA_String_clear(&out);
        fflush(stdout);
    }
    */

    retval = UA_NodeId_parse(&nodeId, UA_STRING_ALLOC(parentId));
    if (retval != UA_STATUSCODE_GOOD) {
        UA_LOG_WARNING(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "Invalid parent ID %s. (%s)", parentId, UA_StatusCode_name(retval));
        UA_Client_disconnect(client);
        UA_Client_delete(client);
        return EXIT_FAILURE;
    }
    retval = scan4Variables(client, nodeId, &variableIds);
    if (retval != UA_STATUSCODE_GOOD) {
        UA_LOG_WARNING(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "Could not retrieve variable node IDs from parent ID %s. (%s)", parentId, UA_StatusCode_name(retval));
    }

    for (UA_NodeId nodeId : variableIds) {
        // print node ID
        UA_print(&nodeId, &UA_TYPES[UA_TYPES_NODEID], &out);
        printf("%.*s\n", (UA_UInt16)out.length, out.data);
        fflush(stdout);
        UA_String_clear(&out);

        // read value of OPC UA variable
        retval = UA_Client_readValueAttribute(client, nodeId, &outValue);
        if (retval != UA_STATUSCODE_GOOD) {
            UA_print(&nodeId, &UA_TYPES[UA_TYPES_NODEID], &out);
            UA_LOG_WARNING(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "Could not read %.*s (%s)", (UA_UInt16)out.length, out.data, UA_StatusCode_name(retval));
            UA_String_clear(&out);
            UA_NodeId_clear(&nodeId);
            UA_Client_disconnect(client);
            UA_Client_delete(client);
            client = UA_Client_new();
            UA_ClientConfig_setDefault(UA_Client_getConfig(client));
            retval = UA_Client_connect(client, uaUrl);
            if (retval == UA_STATUSCODE_GOOD) {
                UA_Client_getConfig(client)->customDataTypes = customDataTypes;
            }
            else {
                UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "Could not reconnect to %s", uaUrl);
            }
            continue;
        }
        // print value of OPC UA variable
        retval = UA_PrintValue(client, nodeId, &outValue, &out);
        if (retval != UA_STATUSCODE_GOOD) {
            UA_print(&nodeId, &UA_TYPES[UA_TYPES_NODEID], &out);
            UA_LOG_WARNING(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "Could not print data of %.*s (%s)", (UA_UInt16)out.length, out.data, UA_StatusCode_name(retval));
            UA_String_clear(&out);
            UA_Variant_clear(&outValue);
            UA_NodeId_clear(&nodeId);
            continue;
        }
        printf("%.*s\n", (UA_UInt16)out.length, out.data);
        fflush(stdout);
        UA_String_clear(&out);
        UA_Variant_clear(&outValue);
        UA_NodeId_clear(&nodeId);
    }
    
    // close OPC UA session
    UA_Client_disconnect(client);
    UA_Client_delete(client);
    return EXIT_SUCCESS;
}
