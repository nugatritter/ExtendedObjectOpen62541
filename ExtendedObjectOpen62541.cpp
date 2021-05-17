/*************************************************************************\
* Copyright (c) 2021 HZB.
* Author: Carsten Winkler carsten.winkler@helmholtz-berlin.de
\*************************************************************************/


#include <open62541/client_config_default.h>
#include <open62541/client_highlevel.h>
#include <open62541/plugin/log_stdout.h>

// git clone https://gitlab.gnome.org/GNOME/libxml2.git
// libxml2\win32>cscript configure.js compiler=msvc debug=no iconv=no
// libxml2\win32>nmake /f Makefile.msvc
#include <libxml/parser.h> 
#include <libxml/tree.h>
#include <libxml/xpath.h>

#include <map>
#include <vector>
#include <string>
#include <cmath>

#include "TailQueue.h"
#include "ExtendedObjectOpen62541.h"
//#ifdef UA_ENABLE_TYPEDESCRIPTION
//    #undef UA_ENABLE_TYPEDESCRIPTION
//#endif

// calculates the additional memory consumption (padding) of structure elements in the RAM
// returns resulting RAM size of whole structure
// this is a sub function of buildUserDataType
UA_Byte calc_struct_padding(std::vector<const UA_DataType*>* structMemberTypes, std::vector<UA_Byte>* paddingSegments);
// retrieves the binary encoding ID of a data type
// returns true if the binary ID could be retrieved
UA_StatusCode getBinaryEncodingId(UA_Client* client, UA_NodeId pvId, UA_NodeId* binaryEncodingId);
// checks whether the data type is an enumeration
UA_Boolean isEnum(UA_NodeId dataTypeId, std::map<UA_UInt32, std::vector<std::map<std::string, std::string>>>* dataTypeAttributes);
// checks whether the xml node is a leaf (child)
UA_Boolean isLeaf(xmlNode* node);
// shortcut for warning message; the node ID is printed before the message
void printWarn(const char* message, UA_NodeId pvId);

// converts dictionary data type tag to OPC data type address (generated open62541 data types)
const UA_DataType* parseDataType(std::string text) {
    if (xmlStrncasecmp((const xmlChar*)("opc:String"), (xmlChar*)text.data(), (UA_UInt32)text.length()) == 0)
        return &(UA_TYPES[UA_TYPES_STRING]);
    if (xmlStrncasecmp((const xmlChar*)("opc:Byte"), (xmlChar*)text.data(), (UA_UInt32)text.length()) == 0)
        return &(UA_TYPES[UA_TYPES_BYTE]);
    if (xmlStrncasecmp((const xmlChar*)("opc:SByte"), (xmlChar*)text.data(), (UA_UInt32)text.length()) == 0)
        return &(UA_TYPES[UA_TYPES_SBYTE]);
    if (xmlStrncasecmp((const xmlChar*)("opc:Boolean"), (xmlChar*)text.data(), (UA_UInt32)text.length()) == 0)
        return &(UA_TYPES[UA_TYPES_BOOLEAN]);
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
    if (xmlStrncasecmp((const xmlChar*)("opc:CharArray"), (xmlChar*)text.data(), (UA_UInt32)text.length()) == 0)
        return &(UA_TYPES[UA_TYPES_STRING]);
    return NULL;
}

// creates user data type from data type structure
// this is a sub function of getUserDataType
void buildUserDataType(UA_DataTypeKind userDataTypeKind, std::vector<const UA_DataType*>* structMemberTypes, UA_DataType* userDataType) {
    std::vector<const UA_DataType*> enumMemberTypes; 
    std::vector<UA_Byte> paddings;
    std::vector<UA_Byte>::iterator padding;
    std::vector<const UA_DataType*>::iterator structMemberType;
    UA_DataTypeMember* userDataTypeMembers = 0x0;
    UA_Byte index = 0;
    UA_Byte size = 0;
    UA_Boolean pointerFree = UA_TRUE;
    structMemberType = structMemberTypes->begin();
    if (structMemberType == structMemberTypes->end())
        return;
    if (userDataTypeKind == UA_DATATYPEKIND_UNION) {
        enumMemberTypes.push_back(*structMemberType);
        enumMemberTypes.push_back(*std::prev(structMemberTypes->end()));
        size = calc_struct_padding(&enumMemberTypes, &paddings);
        if (enumMemberTypes.size() != paddings.size()) // result is corrupt
            return;
        userDataTypeMembers = (UA_DataTypeMember*)UA_malloc((UA_UInt16)enumMemberTypes.size() * sizeof(UA_DataTypeMember));
        memset(userDataTypeMembers, 0x0, (UA_UInt16)enumMemberTypes.size() * sizeof(UA_DataTypeMember));
        structMemberType = enumMemberTypes.begin();
    }
    else {
        size = calc_struct_padding(structMemberTypes, &paddings);
        if (structMemberTypes->size() != paddings.size()) // result is corrupt
            return;
        userDataTypeMembers = (UA_DataTypeMember*)UA_malloc(structMemberTypes->size() * sizeof(UA_DataTypeMember));
        memset(userDataTypeMembers, 0x0, structMemberTypes->size() * sizeof(UA_DataTypeMember));
    }
    padding = paddings.begin();
    while (padding != paddings.end()) {
        userDataTypeMembers[index].isArray = UA_FALSE;
        userDataTypeMembers[index].isOptional = UA_FALSE;
        userDataTypeMembers[index].memberTypeIndex = (*structMemberType)->typeIndex;
        if (userDataTypeMembers[index].memberTypeIndex > UA_TYPES_DOUBLE)
            pointerFree = UA_FALSE;
        userDataTypeMembers[index].namespaceZero = (*structMemberType)->typeId.namespaceIndex == 0 ? UA_TRUE : UA_FALSE;
        userDataTypeMembers[index].padding = *padding;
#ifdef UA_ENABLE_TYPEDESCRIPTION
        size_t length = strlen((*structMemberType)->typeName);
        userDataTypeMembers[index].memberName = (char*)UA_malloc((length + 1) * sizeof(char));
        strncpy((char*)userDataTypeMembers[index].memberName, (*structMemberType)->typeName, length);
        ((char*)userDataTypeMembers[index].memberName)[length] = 0x0;
        //UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "%-10s has a size of %2d bytes, a padding of %2d bytes and a resulting memory consumption of %2d bytes", (*structMemberType)->typeName, (*structMemberType)->memSize, *padding, (*structMemberType)->memSize + *padding);
#endif
        index++;
        padding++;
        structMemberType++;
    }
    userDataType->membersSize = index;
    userDataType->members = userDataTypeMembers;
    if (userDataTypeKind == UA_DATATYPEKIND_UNION)
        userDataType->memSize = size;
    else
        userDataType->memSize = size;
    userDataType->pointerFree = pointerFree;
    userDataType->overlayable = UA_FALSE;
    userDataType->typeKind = userDataTypeKind;
    userDataType->typeIndex = UA_TYPES_EXTENSIONOBJECT;
//#ifdef UA_ENABLE_TYPEDESCRIPTION
//    UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "%-10s has a size of %d bytes", userDataType->typeName, userDataType->memSize, size);
//#endif
}

// helper function to print binary values
void binary_print(UA_Byte value) {
    printf("%c%c%c%c %c%c%c%c %c%c%c%c %c%c%c%c", value & 0x8000 ? '1' : '0', value & 0x4000 ? '1' : '0', value & 0x2000 ? '1' : '0', value & 0x1000 ? '1' : '0',
        value & 0x0800 ? '1' : '0', value & 0x0400 ? '1' : '0', value & 0x0200 ? '1' : '0', value & 0x0100 ? '1' : '0',
        value & 0x0080 ? '1' : '0', value & 0x0040 ? '1' : '0', value & 0x0020 ? '1' : '0', value & 0x0010 ? '1' : '0',
        value & 0x0008 ? '1' : '0', value & 0x0004 ? '1' : '0', value & 0x0002 ? '1' : '0', value & 0x0001 ? '1' : '0');
}

// this function collects reference type ID, forwarded flag, 
// browse name, display name, node class and type definition
// retuns true if browse was successful
UA_StatusCode browseNodeId(UA_Client* client, UA_NodeId nodeId, UA_BrowseResponse* bResp) {
    if (!client) {
        UA_LOG_WARNING(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "client session invalid");
        return UA_STATUSCODE_BAD;
    }
    UA_BrowseRequest bReq;
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
// you have to delete the allocated memory of the returned string
char* byteStringToString(UA_ByteString* bytes) {
    if (!bytes)
        return NULL;
    size_t bufferSize = bytes->length;
    char* str = new char[bufferSize + 1];
    memcpy(str, bytes->data, bufferSize);
    *(str + bufferSize) = 0x0;
    return str;
}

// helper function to remove name space offset
// returns the real name space index
UA_UInt32 getNameSpaceIndex(UA_UInt32 dictMapIndex) {
    return (dictMapIndex - (UA_UInt32)floor((UA_Double)dictMapIndex / UA_NAMESPACEOFFSET) * UA_NAMESPACEOFFSET);
}

// retrieves all dictionaries of the OPC UA server
UA_Int16 getDictionaries(UA_Client* client, std::map<UA_UInt32, std::string>* dictionaries) {
    if (!client) {
        UA_LOG_WARNING(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "client session invalid");
        return 0;
    }
    UA_ReferenceDescription rDesc;
    UA_BrowseResponse bResp;
    dictionaries->clear();
    browseNodeId(client, UA_NODEID_NUMERIC(0, UA_NS0ID_OPCBINARYSCHEMA_TYPESYSTEM), &bResp);
    for (size_t i = 0; i < bResp.resultsSize; ++i) {
        for (size_t j = 0; j < bResp.results[i].referencesSize; ++j) {
            rDesc = bResp.results[i].references[j];
#ifdef EXO_USER_DICIONARIES_ONLY
            if (rDesc.nodeId.nodeId.namespaceIndex != 0) {
#endif // EXO_USER_DICIONARIES_ONLY            
                UA_Variant outValue;
                UA_Client_readValueAttribute(client, rDesc.nodeId.nodeId, &outValue);
                if (outValue.type == &UA_TYPES[UA_TYPES_BYTESTRING]) {
                    char* str = byteStringToString((UA_ByteString*)outValue.data);
                    dictionaries->insert(std::pair<UA_UInt32, std::string>((UA_UInt32)(j * UA_NAMESPACEOFFSET + rDesc.nodeId.nodeId.namespaceIndex), std::string(str)));
                    delete str;
                }
                UA_Variant_clear(&outValue);
#ifdef EXO_USER_DICIONARIES_ONLY
            }
#endif // EXO_USER_DICIONARIES_ONLY  
        }
    }
    return (UA_Int16)dictionaries->size();
}

// prints dictionaries of the OPC UA server
// use the getDictionaries function to retrieve them
void printDictionaries(std::map<UA_UInt32, std::string>* dictionaries) {
    std::map<UA_UInt32, std::string>::iterator it;
    for (it = dictionaries->begin(); it != dictionaries->end(); ++it) {
        UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "namespace: %d", getNameSpaceIndex(it->first));
        UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "%s", it->second.c_str());
    }
}

// search for a OPC UA browse name in an xml node of the dictionary
// writes the found node into the result
// returns true if the tag has been found
UA_Boolean findBrowseName(xmlNode* node, UA_String browseName, xmlNode** result) {
    if (!node || !browseName.length || !result)
        return UA_FALSE;
    UA_Boolean found = UA_FALSE;
    while (node) {
        if (node->type == XML_ELEMENT_NODE) {
            if (!isLeaf(node)) {
                char* nodeName = (char*)node->name;
                char* attributeName = (char*)xmlGetProp(node, (xmlChar*)"Name");
                if (nodeName && attributeName) {
                    if (strncmp(attributeName, (char*)browseName.data, strlen(attributeName)) == 0 && (strstr(nodeName, "EnumeratedType") || strstr(nodeName, "StructuredType"))) {
                        *result = node;
                        found = UA_TRUE;
                        xmlFree(attributeName);
                        break;
                    }
                    xmlFree(attributeName);
                }
            }
        }
        found = findBrowseName(node->children, browseName, result);
        node = node->next;
    }
    return found;
}

// this function collects all properties of a xml node
void collectProperties(xmlNode* node, std::vector<std::map<std::string, std::string>>* vec, UA_Boolean isParent) {
    if(!node || !vec || vec->begin() == vec->end())
        return;
    _xmlAttr* attr; 
    if (isParent) {
        std::map<std::string, std::string>* map;
        map = &(*vec->begin());
        if (!map || map->empty())
            return;
        map->insert(std::pair<std::string, std::string>(std::string(EXO_DATATYPE), std::string((char*)node->name)));
        if (strstr((char*)node->name, "StructuredType")) {
            map->insert(std::pair<std::string, std::string>(std::string(EXO_DATATYPE_NS0ID), std::string(std::to_string(UA_NS0ID_STRUCTUREDEFINITION))));
        }
        else if (strstr((char*)node->name, "EnumeratedType")) {
            map->insert(std::pair<std::string, std::string>(std::string(EXO_DATATYPE_NS0ID), std::string(std::to_string(UA_NS0ID_ENUMDEFINITION))));
        }
        else {
            map->insert(std::pair<std::string, std::string>(std::string(EXO_DATATYPE_NS0ID), std::string("0")));
        }
        return;
    }
    while (node) {
        if (node->type == XML_ELEMENT_NODE) {
            attr = node->properties;
            std::map<std::string, std::string> map = std::map<std::string, std::string>();
            while (attr) {
                xmlChar* value = xmlGetProp(node, attr->name);
                auto pair = std::pair<std::string, std::string>(std::string((char*)attr->name), std::string((char*)value));
                map.insert(pair);
                xmlFree(value);
                attr = attr->next;
            }
            vec->push_back(map);
        }
        node = node->next;
    }
}

// this function builds data attributes and user data type of given node ID
// returns RAM size of user data type
UA_UInt16 getUserDataType(UA_Client* client, UA_NodeId nodeId, std::map<UA_UInt32, std::vector<std::map<std::string, std::string>>>* dataTypeAttributes, UA_DataType* userDataType) {
    UA_StatusCode retval = UA_STATUSCODE_BAD;
    UA_UInt16 memSize = 0;
    UA_NodeId binaryEncodingId;
    UA_DataTypeKind userDataTypeKind;
    UA_UInt16 lengthInBits = 0;    
    std::vector<const UA_DataType*> structMemberTypes;
    std::map<UA_UInt32, std::vector<std::map<std::string, std::string>>>::iterator dataTypeAttribute;
    std::vector<std::map<std::string, std::string>>::iterator dataTypeAttributePropertySegment;
    std::map<std::string, std::string>::iterator dataTypeAttributeProperty;
    retval = getBinaryEncodingId(client, nodeId, &binaryEncodingId);
    if (retval == UA_STATUSCODE_GOOD)
        UA_NodeId_copy(&binaryEncodingId, &userDataType->binaryEncodingId);
    else
        UA_NodeId_init(&binaryEncodingId);
    dataTypeAttribute = dataTypeAttributes->find(UA_NodeId_hash(&userDataType->typeId));
    if (dataTypeAttribute == dataTypeAttributes->end()) {
        printWarn("has inknown data type", nodeId);
        return 0;
    }
    dataTypeAttributePropertySegment = dataTypeAttribute->second.begin();
    if (dataTypeAttributePropertySegment == dataTypeAttribute->second.end()) {
        printWarn("has inknown data type", nodeId);
        return 0;
    }
    dataTypeAttributeProperty = dataTypeAttributePropertySegment->find(EXO_DATATYPE_NS0ID);
    if (dataTypeAttributeProperty == dataTypeAttributePropertySegment->end() || dataTypeAttributeProperty->first.empty() || dataTypeAttributeProperty->second.empty()) {
        printWarn("has inknown data type", nodeId);
        return 0;
    }
    dataTypeAttributePropertySegment++;
    if(dataTypeAttributePropertySegment != dataTypeAttribute->second.end()) {
        switch (std::stoul(dataTypeAttributeProperty->second)) {
        case UA_NS0ID_STRUCTUREDEFINITION: {
            std::string name;
            std::string parentName;
            std::string switchField;
            const UA_DataType* dataType = 0x0;
            const UA_DataType* maxDataType = 0x0;
            UA_UInt16 value = 0;
            UA_UInt16 switchValue = 0;
            userDataTypeKind = UA_DATATYPEKIND_STRUCTURE;
            while (dataTypeAttributePropertySegment != dataTypeAttribute->second.end()) {
                name.clear();
                switchField.clear();
                dataType = 0x0;
                value = 0;
                switchValue = 0;
                dataTypeAttributeProperty = dataTypeAttributePropertySegment->find(EXO_DATANAME);
                if (dataTypeAttributeProperty != dataTypeAttributePropertySegment->end()) {
                    name = std::string(dataTypeAttributeProperty->second);
                    if (parentName.empty())
                        parentName = std::string(dataTypeAttributeProperty->second);
                }
                dataTypeAttributeProperty = dataTypeAttributePropertySegment->find("TypeName");
                if (dataTypeAttributeProperty != dataTypeAttributePropertySegment->end()) {
                    dataType = parseDataType(dataTypeAttributeProperty->second);
                }
                dataTypeAttributeProperty = dataTypeAttributePropertySegment->find("Value");
                if (dataTypeAttributeProperty != dataTypeAttributePropertySegment->end()) {
                    value = std::stoi(dataTypeAttributeProperty->second);
                }
                dataTypeAttributeProperty = dataTypeAttributePropertySegment->find("SwitchValue");
                if (dataTypeAttributeProperty != dataTypeAttributePropertySegment->end()) {
                    switchValue = std::stoi(dataTypeAttributeProperty->second);
                }
                dataTypeAttributeProperty = dataTypeAttributePropertySegment->find("SwitchField");
                if (dataTypeAttributeProperty != dataTypeAttributePropertySegment->end()) {
                    switchField = std::string(dataTypeAttributeProperty->second);
                    userDataTypeKind = UA_DATATYPEKIND_UNION;
                }
                dataTypeAttributeProperty = dataTypeAttributePropertySegment->find("LengthInBits");
                if (dataTypeAttributeProperty != dataTypeAttributePropertySegment->end()) {
                    lengthInBits = std::stoi(dataTypeAttributeProperty->second);
                }
                if (dataType) {
                    if (!switchField.empty() && !parentName.empty() && switchField.compare(parentName) == 0) {
                        if (!maxDataType)
                            maxDataType = dataType;
                        else if (maxDataType->memSize < dataType->memSize)
                            maxDataType = dataType;
                    }
                    structMemberTypes.push_back(dataType);
                }
                dataTypeAttributePropertySegment++;
            };
            if (userDataTypeKind == UA_DATATYPEKIND_UNION && maxDataType)
                structMemberTypes.push_back(maxDataType);
            break;
        }
        case UA_NS0ID_ENUMDEFINITION: {
            printWarn("is an enumeration and no user data type", nodeId);
            return 0;
        }
        default: {
            printWarn("has unsupported data type (1)", nodeId);
            return 0;
        }
        }
    }
    if (structMemberTypes.size() == 0) {
        printWarn("has unsupported data type (2)", nodeId);
    }
    else {
        buildUserDataType(userDataTypeKind, &structMemberTypes, userDataType);
        if (userDataType)
            memSize = userDataType->memSize;
        else
            printWarn("has unsupported data type (3)", nodeId);
    }
    if (lengthInBits && lengthInBits != memSize) {
        printWarn("data type has corrupt memory size", nodeId);
        return 0;
    }
    return memSize;
}

// retrieves the property set from a OPC UA browse response
// use the function browseNodeId to get the parameter UA_BrowseResponse* bResp
void getPropertyDescription(UA_Client* client, UA_BrowseResponse* bResp, std::vector<std::string>* propertySet) {
    UA_ReferenceDescription rDesc;
    for (size_t i = 0; i < bResp->resultsSize; ++i) {
        for (size_t j = 0; j < bResp->results[i].referencesSize; ++j) {
            rDesc = bResp->results[i].references[j];
            if (rDesc.referenceTypeId.identifierType == UA_NODEIDTYPE_NUMERIC && rDesc.referenceTypeId.identifier.numeric == UA_NS0ID_HASPROPERTY) {
                UA_Variant outValue;
                UA_Client_readValueAttribute(client, rDesc.nodeId.nodeId, &outValue);
                if (!UA_Variant_isScalar(&outValue)) {
                    if (outValue.type->typeId.namespaceIndex == 0 && outValue.type->typeId.identifierType == UA_NODEIDTYPE_NUMERIC && outValue.type->typeId.identifier.numeric == UA_NS0ID_LOCALIZEDTEXT) {
                        UA_LocalizedText* data = (UA_LocalizedText*)outValue.data;
                        uintptr_t target = (uintptr_t)outValue.arrayDimensions;
                        for (UA_UInt32 i = 0; i < (UA_UInt32)outValue.arrayLength; i++) {                           
                            propertySet->push_back(std::string((char*)data[i].text.data, data[i].text.length));
                        }
                    }
                }
                UA_Variant_clear(&outValue);
            }
        }
    }
}

// retrieves the attributes of a node ID (hash value)
void getAttributes(xmlNode* node, UA_UInt32 typeHash, std::map<UA_UInt32, std::vector<std::map<std::string, std::string>>>* dataTypeAttributes) {
    if (!node)
        return;
    std::vector<std::map<std::string, std::string>> vec;
    std::map<std::string, std::string> map;
    xmlChar* parentName = xmlGetProp(node, (const xmlChar*)("Name"));
    if (parentName) {
        map.insert(std::pair<std::string, std::string>(std::string(EXO_DATANAME), std::string((char*)parentName)));
        vec.push_back(map);
        collectProperties(node, &vec, UA_TRUE);
        collectProperties(node->children, &vec, UA_FALSE);
        auto pair = std::pair<UA_UInt32, std::vector<std::map<std::string, std::string>>>(typeHash, vec);
        dataTypeAttributes->insert(pair);
    }
}

// retrieves the binary encoding ID of a data type
// returns true if the binary ID could be retrieved
UA_StatusCode getBinaryEncodingId(UA_Client* client, UA_NodeId pvId, UA_NodeId* binaryEncodingId) {
    if (!client) {
        UA_LOG_WARNING(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "client session invalid");
        return UA_STATUSCODE_BAD;
    }
    if (!binaryEncodingId) {
        UA_LOG_WARNING(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "binaryEncodingId must not be NULL");
        return UA_STATUSCODE_BAD;
    }
    UA_Variant outValue;
    UA_ExtensionObject* data;
    UA_StatusCode retval = UA_Client_readValueAttribute(client, pvId, &outValue);
    if (retval != UA_STATUSCODE_GOOD) {
        printWarn("failed to read value attributes", pvId);
        return retval;
    }
    if (outValue.type->typeIndex != UA_TYPES_EXTENSIONOBJECT) {
        printWarn("has no data type index for extension object", pvId);
        return UA_STATUSCODE_BAD;
    }
    data = (UA_ExtensionObject*)outValue.data;
    retval = UA_NodeId_copy(&data->content.encoded.typeId, binaryEncodingId);
    return retval;
}

// retrieves all member names of a structure of a user data type
// this is a sub function of getStructureValues
void getStructureFieldNames(UA_DataType* userDataType, std::map<UA_UInt32, std::vector<std::map<std::string, std::string>>>* dataTypeAttributes, std::vector<std::string>* fieldNames) {
    UA_UInt16 foundCount = 0;
    std::map<UA_UInt32, std::vector<std::map<std::string, std::string>>>::iterator dataTypeAttribute;
    std::vector<std::map<std::string, std::string>>::iterator dataTypeAttributePropertySegment;
    std::map<std::string, std::string>::iterator dataTypeAttributeProperty;
    std::vector<std::map<std::string, std::string>> attribues;
    for (dataTypeAttribute = dataTypeAttributes->begin(); dataTypeAttribute != dataTypeAttributes->end(); ++dataTypeAttribute) {
        if (dataTypeAttribute->first == UA_NodeId_hash(&userDataType->typeId)) {
            attribues = dataTypeAttribute->second;
            for (dataTypeAttributePropertySegment = attribues.begin(); dataTypeAttributePropertySegment != attribues.end(); dataTypeAttributePropertySegment++) {
                foundCount = 0;
                std::string fieldName;
                for (dataTypeAttributeProperty = dataTypeAttributePropertySegment->begin(); dataTypeAttributeProperty != dataTypeAttributePropertySegment->end(); dataTypeAttributeProperty++) {
                    if (xmlStrncasecmp((const xmlChar*)("TypeName"), (xmlChar*)dataTypeAttributeProperty->first.data(), (UA_UInt32)dataTypeAttributeProperty->first.length()) == 0)
                        foundCount++;
                    else if (xmlStrncasecmp((const xmlChar*)("Name"), (xmlChar*)dataTypeAttributeProperty->first.data(), (UA_UInt32)dataTypeAttributeProperty->first.length()) == 0) {
                        fieldName = std::string(dataTypeAttributeProperty->second);
                        foundCount++;
                    }
                    if (foundCount == 2)
                        break;
                }
                if (foundCount == 2) {
                    fieldNames->push_back(fieldName);
                }
            }
        }
    }
}

// retrieves all member names and values of a structure of a user data type
// returns true if successfully
UA_StatusCode getStructureValues(UA_Client* client, UA_Variant structVal, UA_DataType* userDataType, std::map<UA_UInt32, std::vector<std::map<std::string, std::string>>>* dataTypeAttributes, std::map<std::string, UA_Variant>* structureMembers) {
    const UA_DataType* mt;
    const UA_DataType* typelists[2] = { UA_TYPES, &userDataType[-userDataType->typeIndex] };
    const UA_DataTypeMember* m;
    std::map<std::string, UA_Variant>::iterator it;
    std::vector<std::string> fieldNames;
    std::vector<std::string> propertySet;
    std::vector<std::string>::iterator propertySetIt;
    UA_Boolean* variantContent = 0x0;
    UA_BrowseResponse bResp1;
    UA_Byte pos = 0;
    UA_Byte resultingBits = 0;
    UA_Byte* pValidBits = 0x0;
    UA_Byte* pValue = 0x0;
    UA_StatusCode retval = UA_STATUSCODE_GOOD;
    UA_Variant* validBits = 0x0;
    UA_Variant* value = 0x0;
    uintptr_t ptrs = (uintptr_t)structVal.data;

    getStructureFieldNames(userDataType, dataTypeAttributes, &fieldNames);
    if (!UA_NodeId_equal(&structVal.type->typeId, &userDataType->typeId)) {
        UA_LOG_WARNING(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "Data types don't match");
        return UA_STATUSCODE_BAD;
    }
    if (ptrs && ((UA_ExtensionObject*)ptrs)->encoding == UA_EXTENSIONOBJECT_ENCODED_BYTESTRING) {
        if (fieldNames.size() == userDataType->membersSize) {
            for (UA_UInt32 i = 0; i < userDataType->membersSize; ++i) {
                m = &userDataType->members[i];
                mt = &typelists[!m->namespaceZero][m->memberTypeIndex];
                ptrs += m->padding;
                if (!m->isArray) {
                    UA_Variant varVal;
                    UA_Variant_init(&varVal);
                    retval = UA_Variant_setScalarCopy(&varVal, (void*)ptrs, mt);
                    if(retval == UA_STATUSCODE_GOOD)
                        structureMembers->insert(std::pair<std::string, UA_Variant>(std::string(fieldNames[i]), varVal));
                    ptrs += mt->memSize;
                }
            }
            if (structureMembers->size() >= 2) {
                it = structureMembers->begin();
                value = &it->second;
                it++;
                validBits = &it->second;
                pValue = ((UA_ByteString*)value->data)->data;
                /*printf("%-25s: ", "value");
                binary_print(*pValue);
                printf(" (%d)\n", *pValue);*/
                pValidBits = ((UA_ByteString*)validBits->data)->data;
               /* printf("%-25s: ", "valid bits");
                binary_print(*pValidBits);
                printf(" (%d)\n", *pValidBits);*/
                resultingBits = *pValue & *pValidBits;
                /*printf("%-25s: ", "result");
                binary_print(resultingBits);
                printf(" (%d)\n", resultingBits);*/
                retval = browseNodeId(client, userDataType->typeId, &bResp1);
                if (retval == UA_STATUSCODE_GOOD) {
                    getPropertyDescription(client, &bResp1, &propertySet);
                    structureMembers->clear();
                    for (propertySetIt = propertySet.begin(); propertySetIt != propertySet.end(); ++propertySetIt) {
                        //printf("%-25s: %s\n", propertySetIt->c_str(), (resultingBits & 0x01 << pos) ? "TRUE" : "FALSE");
                        UA_Variant varVal;
                        UA_Variant_init(&varVal);
                        variantContent = UA_Boolean_new();
                        *variantContent = resultingBits & 0x01 << pos++;
                        UA_Variant_setScalar(&varVal, variantContent, &UA_TYPES[UA_TYPES_BOOLEAN]);                   
                        structureMembers->insert(std::pair<std::string, UA_Variant>(std::string(*propertySetIt), varVal));
                    }
                }
                else {
                    printWarn("could not retrieve property descriptions", userDataType->typeId);
                }
            }
        }
    }
    else {
        if (fieldNames.size() == userDataType->membersSize) {
            for (UA_UInt32 i = 0; i < userDataType->membersSize; ++i) {
                m = &userDataType->members[i];
                mt = &typelists[!m->namespaceZero][m->memberTypeIndex];
                ptrs += m->padding;
                UA_Variant varVal;
                UA_Variant_init(&varVal);
                UA_Variant_setScalarCopy(&varVal, (void*)ptrs, mt);
                if (!m->isArray) {
                    structureMembers->insert(std::pair<std::string, UA_Variant>(std::string(fieldNames[i]), varVal));
                    ptrs += mt->memSize;
                }
            }
        }
    }
    return retval;
}

// retrieves the data type ID of a node ID
// returns true if successfully
UA_StatusCode getTypeId(UA_Client* client, UA_NodeId pvId, UA_NodeId* pvTypeId) {
    if (!client) {
        UA_LOG_WARNING(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "client session invalid");
        return UA_STATUSCODE_BAD;
    }
    return UA_Client_readDataTypeAttribute(client, pvId, pvTypeId);
}

// converts raw OPC UA dictionaries to xml documents
void getXmlDocMap(std::map<UA_UInt32, std::string>* dictionaries, std::map<UA_UInt32, xmlDocPtr>* docs) {
    if (!docs)
        return;
    xmlDocPtr doc;
    std::map<UA_UInt32, std::string>::iterator it;
    for (it = dictionaries->begin(); it != dictionaries->end(); ++it) {
        doc = xmlReadMemory((const char*)it->second.c_str(), (UA_UInt32)it->second.length(), "include.xml", NULL, 0);
        if (doc != NULL) {
            docs->insert(std::pair<UA_UInt32, xmlDocPtr>(it->first, doc));
        }
    }
}

// retrieves user data type and user data type attributes of a node ID
// returns UA_STATUSCODE_UNCERTAIN for an enumeration, UA_STATUSCODE_GOOD for a structure and UA_STATUSCODE_BAD if no user data type was found
UA_StatusCode getUserDataTypeAttributes(UA_Client* client, UA_NodeId pvId, std::map<UA_UInt32, std::string>* dictionaries, UA_DataType* userDataType, std::map<UA_UInt32, std::vector<std::map<std::string, std::string>>>* dataTypeAttributes) {
    if (!client) {
        UA_LOG_WARNING(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "client session invalid");
        return UA_STATUSCODE_BAD;
    }
    if (UA_NodeId_isNull(&pvId)) {
        UA_LOG_WARNING(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "Node ID must not be NULL");
        return UA_STATUSCODE_BAD;
    }
    UA_NodeId dataTypeId;
    UA_QualifiedName browseName;
    UA_StatusCode retval;
    std::map<UA_UInt32, xmlDocPtr> xmlDocMap;
    std::map<UA_UInt32, xmlDocPtr>::iterator xmlDocNode;
    xmlNode* attributeNode = NULL;
    xmlDocNode = xmlDocMap.begin();
    dataTypeAttributes->clear();
    UA_NodeId_init(&dataTypeId);
    UA_QualifiedName_init(&browseName);
    retval = getTypeId(client, pvId, &dataTypeId);
    if (retval != UA_STATUSCODE_GOOD) {
        printWarn("failed to read value type node ID", pvId);
        UA_QualifiedName_clear(&browseName);
        return retval;
    }
    if (dataTypeId.namespaceIndex == 0) {
        char warn[256];
        UA_String out;
        UA_print(&dataTypeId, &UA_TYPES[UA_TYPES_NODEID], &out);
        sprintf(warn, "has no user data type (Node ID of data type is %.*s)", (UA_UInt16)out.length, out.data);
        printWarn(warn, pvId);
        UA_String_clear(&out);
        UA_QualifiedName_clear(&browseName);
        return UA_STATUSCODE_BAD;
    }
    retval = UA_Client_readBrowseNameAttribute(client, dataTypeId, &browseName);
    if (retval != UA_STATUSCODE_GOOD || !browseName.name.length) {
        printWarn("failed to read browse name", pvId);
        UA_QualifiedName_clear(&browseName);
        return retval;
    }
    getXmlDocMap(dictionaries, &xmlDocMap);
    retval = UA_STATUSCODE_BAD;
    for (xmlDocNode = xmlDocMap.begin(); xmlDocNode != xmlDocMap.end(); ++xmlDocNode) {
        xmlNode* root_element = xmlDocGetRootElement(xmlDocNode->second);
        if (findBrowseName(root_element, browseName.name, &attributeNode)) {
            getAttributes(attributeNode, UA_NodeId_hash(&dataTypeId), dataTypeAttributes);
            retval = UA_STATUSCODE_GOOD;
            break;
        }
    }
    UA_NodeId_copy(&dataTypeId, &userDataType->typeId);
#ifdef UA_ENABLE_TYPEDESCRIPTION
        userDataType->typeName = (char*)malloc((browseName.name.length + 1) * sizeof(char));
        if (userDataType->typeName) {
            strncpy((char*)userDataType->typeName, (char*)browseName.name.data, browseName.name.length);
            ((char*)userDataType->typeName)[browseName.name.length] = 0x0;
        }
#endif   
    if (isEnum(dataTypeId, dataTypeAttributes)) {
        retval = UA_STATUSCODE_UNCERTAIN;
    } else if (!getUserDataType(client, pvId, dataTypeAttributes, userDataType)) {
        retval = UA_STATUSCODE_BAD;
    }
    UA_NodeId_clear(&dataTypeId);
    UA_QualifiedName_clear(&browseName);
    return retval;
}

// checks whether the data type is an enumeration
UA_Boolean isEnum(UA_NodeId dataTypeId, std::map<UA_UInt32, std::vector<std::map<std::string, std::string>>>* dataTypeAttributes) {
    std::map<UA_UInt32, std::vector<std::map<std::string, std::string>>>::iterator dataTypeAttribute;
    std::vector<std::map<std::string, std::string>>::iterator dataTypeAttributePropertySegment;
    std::map<std::string, std::string>::iterator dataTypeAttributeProperty;
    dataTypeAttribute = dataTypeAttributes->find(UA_NodeId_hash(&dataTypeId));
    if (dataTypeAttribute == dataTypeAttributes->end())
        return UA_FALSE;
    dataTypeAttributePropertySegment = dataTypeAttribute->second.begin();
    if (dataTypeAttributePropertySegment == dataTypeAttribute->second.end())
        return UA_FALSE;
    dataTypeAttributeProperty = dataTypeAttributePropertySegment->find(EXO_DATATYPE_NS0ID);
    if (dataTypeAttributeProperty == dataTypeAttributePropertySegment->end())
        return UA_FALSE;
    if (std::stoul(dataTypeAttributeProperty->second) == UA_NS0ID_ENUMDEFINITION)
        return UA_TRUE;
    return UA_FALSE;
}

// checks whether the xml node is a leaf (child)
UA_Boolean isLeaf(xmlNode* node) {
    if (!node)
        return UA_FALSE;
    xmlNode* child = node->children;
    while (child) {
        if (child->type == XML_ELEMENT_NODE) return UA_FALSE;
        child = child->next;
    }
    return UA_TRUE;
}

// prints all user data attributes of given user data type
void printUserDataTypeAttributes(std::map<UA_UInt32, std::vector<std::map<std::string, std::string>>>* dataTypeAttributes, UA_DataType* dataType) {
    if (UA_NodeId_equal(&dataType->typeId, &UA_NODEID_NULL))
        return;
    std::map<UA_UInt32, std::vector<std::map<std::string, std::string>>>::iterator dataTypeAttribute;
    std::vector<std::map<std::string, std::string>>::iterator dataTypeAttributePropertySegment;
    std::map<std::string, std::string>::iterator dataTypeAttributeProperty;
    dataTypeAttribute = dataTypeAttributes->find(UA_NodeId_hash(&dataType->typeId));
    if (dataTypeAttribute != dataTypeAttributes->end()) {
        dataTypeAttributePropertySegment = (dataTypeAttribute->second).begin();
        if (dataTypeAttributePropertySegment != (dataTypeAttribute->second).end()) {
            dataTypeAttributeProperty = dataTypeAttributePropertySegment->find(EXO_DATANAME);
            if (dataTypeAttributeProperty != dataTypeAttributePropertySegment->end()) {
                UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "%s", dataTypeAttributeProperty->second.c_str());
                for (dataTypeAttributePropertySegment = dataTypeAttribute->second.begin(); dataTypeAttributePropertySegment != dataTypeAttribute->second.end(); ++dataTypeAttributePropertySegment) {
                    for (dataTypeAttributeProperty = dataTypeAttributePropertySegment->begin(); dataTypeAttributeProperty != dataTypeAttributePropertySegment->end(); ++dataTypeAttributeProperty) {
                        UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "   %s = %s", dataTypeAttributeProperty->first.c_str(), dataTypeAttributeProperty->second.c_str());
                    }
                    UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "------------------------------------------------------");
                }
            }
        }
    }
    else {
        UA_LOG_WARNING(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "DataType not found");
    }
}

// shortcut for warning message; the node ID is printed before the message
void printWarn(const char* message, UA_NodeId pvId) {
    UA_String out;
    UA_print(&pvId, &UA_TYPES[UA_TYPES_NODEID], &out);
    UA_LOG_WARNING(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "%.*s %s ", (UA_UInt32)out.length, out.data, message);
    UA_String_clear(&out);
}

// helper function of tail queue
static UA_PrintOutput* UA_PrintContext_addOutput(UA_PrintContext* ctx, size_t length) {
    /* Protect against overlong output in pretty-printing */
    if (length > 2 << 16)
        return NULL;
    UA_PrintOutput* output = (UA_PrintOutput*)UA_malloc(sizeof(UA_PrintOutput) + length + 1);
    if (!output)
        return NULL;
    output->length = length;
    TAILQ_INSERT_TAIL(&ctx->outputs, output, next);
    return output;
}

// helper function of tail queue
static UA_StatusCode UA_PrintContext_addNewlineTabs(UA_PrintContext* ctx, size_t tabs) {
    UA_PrintOutput* out = UA_PrintContext_addOutput(ctx, tabs + 1);
    if (!out)
        return UA_STATUSCODE_BADOUTOFMEMORY;
    out->data[0] = '\n';
    for (size_t i = 1; i <= tabs; i++)
        out->data[i] = '\t';
    return UA_STATUSCODE_GOOD;
}

// helper function of tail queue
static UA_StatusCode UA_PrintContext_addName(UA_PrintContext* ctx, const char* name) {
    size_t nameLen = strlen(name);
    UA_PrintOutput* out = UA_PrintContext_addOutput(ctx, nameLen + 2);
    if (!out)
        return UA_STATUSCODE_BADOUTOFMEMORY;
    memcpy(&out->data, name, nameLen);
    out->data[nameLen] = ':';
    out->data[nameLen + 1] = ' ';
    return UA_STATUSCODE_GOOD;
}

// helper function of tail queue
static UA_StatusCode UA_PrintContext_addString(UA_PrintContext* ctx, const char* str) {
    size_t len = strlen(str);
    UA_PrintOutput* out = UA_PrintContext_addOutput(ctx, len);
    if (!out)
        return UA_STATUSCODE_BADOUTOFMEMORY;
    memcpy(&out->data, str, len);
    return UA_STATUSCODE_GOOD;
}

// helper function of tail queue
static UA_StatusCode printNodeId(UA_PrintContext* ctx, const UA_NodeId* p, const UA_DataType* _) {
    UA_String out;
    UA_String_init(&out);
    UA_StatusCode res = UA_NodeId_print(p, &out);
    if (res != UA_STATUSCODE_GOOD)
        return res;
    UA_PrintOutput* po = UA_PrintContext_addOutput(ctx, out.length);
    if (po)
        memcpy(po->data, out.data, out.length);
    else
        res = UA_STATUSCODE_BADOUTOFMEMORY;
    UA_String_clear(&out);
    return res;
}

// helper function of tail queue
static UA_StatusCode printString(UA_PrintContext* ctx, const UA_String* p, const UA_DataType* _) {
    if (!p->data)
        return UA_PrintContext_addString(ctx, "NullString");
    UA_PrintOutput* out = UA_PrintContext_addOutput(ctx, p->length + 2);
    if (!out)
        return UA_STATUSCODE_BADOUTOFMEMORY;
    UA_snprintf((char*)out->data, p->length + 3, "\"%.*s\"", (int)p->length, p->data);
    return UA_STATUSCODE_GOOD;
}

// helper function of tail queue
static UA_StatusCode printByteString(UA_PrintContext* ctx, const UA_ByteString* p, const UA_DataType* _) {
    if (!p->data)
        return UA_PrintContext_addString(ctx, "NullByteString");
    UA_String str = UA_BYTESTRING_NULL;
    UA_StatusCode res = UA_ByteString_toBase64(p, &str);
    if (res != UA_STATUSCODE_GOOD)
        return res;
    res = printString(ctx, &str, NULL);
    UA_String_clear(&str);
    return res;
}

// prints variant data type ENUM to UA_String
static UA_StatusCode UA_printEnum(const void* pData, UA_DataType* userDataType, std::map<UA_UInt32, std::vector<std::map<std::string, std::string>>>* dataTypeAttributes, UA_String* output) {
    std::map<UA_UInt32, std::vector<std::map<std::string, std::string>>>::iterator dataTypeAttribute;
    std::vector<std::map<std::string, std::string>>::iterator dataTypeAttributePropertySegment;
    std::map<std::string, std::string>::iterator dataTypeAttributeProperty;
    std::map<std::string, std::string>::iterator dataTypeAttributeProperty2;
    UA_StatusCode retval = UA_STATUSCODE_GOOD;
    UA_PrintContext ctx = UA_PrintContext();
    ctx.depth = 0;
    TAILQ_INIT(&ctx.outputs);
    UA_Variant* p = (UA_Variant*)pData;
    UA_UInt32 value = 0;
    UA_String_init(output);
    if (!p->type)
        return UA_PrintContext_addString(&ctx, "NullVariant");
    retval |= UA_PrintContext_addString(&ctx, "{");
    ctx.depth++;

    retval |= UA_PrintContext_addNewlineTabs(&ctx, ctx.depth);
    retval |= UA_PrintContext_addName(&ctx, "DataType");
#ifdef UA_ENABLE_TYPEDESCRIPTION
    retval |= UA_PrintContext_addString(&ctx, p->type->typeName);
#endif
    retval |= UA_PrintContext_addString(&ctx, " (ENUM),");

    retval |= UA_PrintContext_addNewlineTabs(&ctx, ctx.depth);
    retval |= UA_PrintContext_addName(&ctx, "Value");
    if (UA_Variant_isScalar(p)) {
        retval |= UA_PrintContext_addString(&ctx, "{");
        ctx.depth++;
        value = *((UA_UInt32*)p->data);
        dataTypeAttribute = dataTypeAttributes->find(UA_NodeId_hash(&userDataType->typeId));
        if (dataTypeAttribute == dataTypeAttributes->end())
            return UA_STATUSCODE_BAD;
        dataTypeAttributePropertySegment = dataTypeAttribute->second.begin();
        if (dataTypeAttributePropertySegment == dataTypeAttribute->second.end())
            return UA_STATUSCODE_BAD;
        dataTypeAttributeProperty = dataTypeAttributePropertySegment->find(EXO_DATATYPE_NS0ID);
        if (dataTypeAttributeProperty == dataTypeAttributePropertySegment->end())
            return UA_STATUSCODE_BAD;

        if (std::stoul(dataTypeAttributeProperty->second) == UA_NS0ID_ENUMDEFINITION) {
            for (; dataTypeAttributePropertySegment != dataTypeAttribute->second.end(); dataTypeAttributePropertySegment++) {
                for (dataTypeAttributeProperty = dataTypeAttributePropertySegment->begin(); dataTypeAttributeProperty != dataTypeAttributePropertySegment->end(); dataTypeAttributeProperty++) {
                    UA_Boolean isValidNumber = UA_TRUE;
                    for (UA_UInt32 i = 0; i < dataTypeAttributeProperty->second.length(); i++) {
                        if (dataTypeAttributeProperty->second.at(i) < 48 || dataTypeAttributeProperty->second.at(i) > 57) {
                            isValidNumber = UA_FALSE;
                            break;
                        }
                    }
                    if (isValidNumber && std::stoi(dataTypeAttributeProperty->second) == value) {
                        if((dataTypeAttributeProperty2 = dataTypeAttributePropertySegment->find(EXO_DATANAME)) == dataTypeAttributePropertySegment->end())
                            continue;
                        retval |= UA_PrintContext_addNewlineTabs(&ctx, ctx.depth);
                        UA_PrintOutput* out = UA_PrintContext_addOutput(&ctx, dataTypeAttributeProperty->second.length());
                        if (!out)
                            retval |= UA_STATUSCODE_BADOUTOFMEMORY;
                        else
                            memcpy(&out->data, dataTypeAttributeProperty->second.c_str(), dataTypeAttributeProperty->second.length());
                        retval |= UA_PrintContext_addString(&ctx, " (");
                        out = UA_PrintContext_addOutput(&ctx, dataTypeAttributeProperty2->second.length());
                        if (!out)
                            retval |= UA_STATUSCODE_BADOUTOFMEMORY;
                        else
                            memcpy(&out->data, dataTypeAttributeProperty2->second.c_str(), dataTypeAttributeProperty2->second.length());
                        retval |= UA_PrintContext_addString(&ctx, ")");
                    }
                }
            }
        }
        

        /*for (const std::pair<std::string, UA_Variant>& valSet : *structureMembers) {
            retval |= UA_PrintContext_addNewlineTabs(&ctx, ctx.depth);
            const UA_DataType* type = &UA_TYPES[valSet.second.type->typeIndex];
            size_t len = valSet.first.length();
            UA_PrintOutput* out = UA_PrintContext_addOutput(&ctx, len);
            if (!out)
                retval |= UA_STATUSCODE_BADOUTOFMEMORY;
            else
                memcpy(&out->data, valSet.first.c_str(), len);
            retval |= UA_PrintContext_addString(&ctx, ": ");
            UA_print(valSet.second.data, type, &outString);
            len = outString.length;
            out = UA_PrintContext_addOutput(&ctx, len);
            if (!out)
                retval |= UA_STATUSCODE_BADOUTOFMEMORY;
            else
                memcpy(&out->data, outString.data, outString.length);
            UA_String_clear(&outString);
        }*/
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

// prints variant data type STRUCTURE to UA_String
static UA_StatusCode UA_printStructure(const void* pData, std::map<std::string, UA_Variant>* structureMembers, UA_String* output) {
    UA_StatusCode retval = UA_STATUSCODE_GOOD;
    UA_PrintContext ctx = UA_PrintContext();
    ctx.depth = 0;
    TAILQ_INIT(&ctx.outputs);
    UA_Variant* p = (UA_Variant*)pData;
    UA_String_init(output);
    if (!p->type)
        return UA_PrintContext_addString(&ctx, "NullVariant");
    retval |= UA_PrintContext_addString(&ctx, "{");
    ctx.depth++;

    retval |= UA_PrintContext_addNewlineTabs(&ctx, ctx.depth);
    retval |= UA_PrintContext_addName(&ctx, "DataType");
#ifdef UA_ENABLE_TYPEDESCRIPTION
    retval |= UA_PrintContext_addString(&ctx, p->type->typeName);
#endif
    retval |= UA_PrintContext_addString(&ctx, ",");

    retval |= UA_PrintContext_addNewlineTabs(&ctx, ctx.depth);
    retval |= UA_PrintContext_addName(&ctx, "Value");
    if (UA_Variant_isScalar(p)) {
        retval |= UA_PrintContext_addString(&ctx, "{");
        ctx.depth++;
        UA_String outString;
        for (const std::pair<std::string, UA_Variant>& valSet : *structureMembers) {
            retval |= UA_PrintContext_addNewlineTabs(&ctx, ctx.depth);
            const UA_DataType* type = &UA_TYPES[valSet.second.type->typeIndex];
            size_t len =valSet.first.length();
            UA_PrintOutput* out = UA_PrintContext_addOutput(&ctx, len);
            if (!out)
                retval |= UA_STATUSCODE_BADOUTOFMEMORY;
            else
                memcpy(&out->data, valSet.first.c_str(), len);
            retval |= UA_PrintContext_addString(&ctx, ": ");
            UA_print(valSet.second.data, type, &outString);
            len = outString.length;
            out = UA_PrintContext_addOutput(&ctx, len);
            if (!out)
                retval |= UA_STATUSCODE_BADOUTOFMEMORY;
            else
                memcpy(&out->data, outString.data, outString.length);
            UA_String_clear(&outString);
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

// prints variant data type UNION to UA_String
static UA_StatusCode UA_printUnion(const void* pData, std::map<UA_UInt32, std::vector<std::map<std::string, std::string>>>* dataTypeAttributes, UA_String* output) {
    UA_StatusCode retval = UA_STATUSCODE_GOOD;
    UA_PrintContext ctx = UA_PrintContext();
    ctx.depth = 0;
    TAILQ_INIT(&ctx.outputs);
    UA_Variant* p =(UA_Variant*)pData;
    UA_String_init(output);
    if (!p->type)
        return UA_PrintContext_addString(&ctx, "NullVariant");
    retval |= UA_PrintContext_addString(&ctx, "{");
    ctx.depth++;

    retval |= UA_PrintContext_addNewlineTabs(&ctx, ctx.depth);
    retval |= UA_PrintContext_addName(&ctx, "DataType");
#ifdef UA_ENABLE_TYPEDESCRIPTION
    retval |= UA_PrintContext_addString(&ctx, p->type->typeName);
#endif
    retval |= UA_PrintContext_addString(&ctx, ",");

    retval |= UA_PrintContext_addNewlineTabs(&ctx, ctx.depth);
    retval |= UA_PrintContext_addName(&ctx, "Value");
    if (UA_Variant_isScalar(p)) {
        UA_UInt32 switchIndex = *((UA_UInt32*)p->data);
        char out[32];
        UA_snprintf(out, 32, "switch index: %d", switchIndex);
        retval |= UA_PrintContext_addString(&ctx, "{");
        ctx.depth++;
        retval |= UA_PrintContext_addNewlineTabs(&ctx, ctx.depth);
        retval |= UA_PrintContext_addString(&ctx, out);
        retval |= UA_PrintContext_addNewlineTabs(&ctx, ctx.depth);
        if (switchIndex <= p->type->membersSize) {
            std::map<UA_UInt32, std::vector<std::map<std::string, std::string>>>::iterator dataTypeAttribute;
            std::vector<std::map<std::string, std::string>>::iterator dataTypeAttributePropertySegment;
            std::map<std::string, std::string>::iterator dataTypeAttributeProperty;
            dataTypeAttribute = dataTypeAttributes->find(UA_NodeId_hash(&p->type->typeId));
            if (dataTypeAttribute != dataTypeAttributes->end()) {
                for(dataTypeAttributePropertySegment = dataTypeAttribute->second.begin(); dataTypeAttributePropertySegment != dataTypeAttribute->second.end(); dataTypeAttributePropertySegment++) {
                    dataTypeAttributeProperty = dataTypeAttributePropertySegment->find("SwitchValue");
                    if (dataTypeAttributeProperty == dataTypeAttributePropertySegment->end())
                        continue;
                    UA_UInt32 index = std::stoi(dataTypeAttributeProperty->second);
                    if (switchIndex != index)
                        continue;
                    dataTypeAttributeProperty = dataTypeAttributePropertySegment->find(EXO_DATANAME);
                    if (dataTypeAttributeProperty == dataTypeAttributePropertySegment->end())
                        continue;
                    size_t len = dataTypeAttributeProperty->second.length();
                    UA_PrintOutput* out = UA_PrintContext_addOutput(&ctx, len);
                    if (!out)
                        retval |= UA_STATUSCODE_BADOUTOFMEMORY;
                    else
                        memcpy(&out->data, dataTypeAttributeProperty->second.c_str(), dataTypeAttributeProperty->second.length());
                    retval |= UA_PrintContext_addString(&ctx, ": ");
                    dataTypeAttributeProperty = dataTypeAttributePropertySegment->find("TypeName");
                    if (dataTypeAttributeProperty == dataTypeAttributePropertySegment->end())
                        continue;
                    const UA_DataType* dataType = parseDataType(dataTypeAttributeProperty->second);
                    UA_String outString;
                    UA_print((UA_Byte*)p->data + sizeof(UA_UInt32) + p->type->members[switchIndex - 1].padding, dataType/*&UA_TYPES[p->type->members[switchIndex - 1].memberTypeIndex]*/, &outString);
                    len = outString.length;
                    out = UA_PrintContext_addOutput(&ctx, len);
                    if (!out)
                        retval |= UA_STATUSCODE_BADOUTOFMEMORY;
                    else
                        memcpy(&out->data, outString.data, outString.length);
                    UA_String_clear(&outString);
                }
            }
        }
        else {
            retval |= UA_PrintContext_addString(&ctx, "Union switch index is invalid");
        }
    ctx.depth--;
        retval |= UA_PrintContext_addNewlineTabs(&ctx, ctx.depth);
    }
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

// calculates the additional memory consumption (padding) of structure elements in the RAM
// returns resulting RAM size of whole structure
// this is a sub function of buildUserDataType
UA_Byte calc_struct_padding(std::vector<const UA_DataType*>* structMemberTypes, std::vector<UA_Byte>* paddingSegments) {
    UA_Byte maxVal = 0;
    UA_Byte size = 0;
    UA_Byte bytes = 0;
    UA_Byte padding = 0;
    UA_Byte currentMemoryBank = 0;
    for(const UA_DataType* dataType : *structMemberTypes) {
        padding = 0;
        bytes = (UA_Byte)dataType->memSize;
        if (bytes > maxVal)
            maxVal = bytes;
        if (bytes > 1 && currentMemoryBank && bytes + currentMemoryBank > ADDRESS_SIZE) {
            padding = ADDRESS_SIZE - currentMemoryBank;
        }
        else if (bytes > 1 && currentMemoryBank % 2) {
            padding = 1;
        }
        currentMemoryBank += bytes + padding;
        size += bytes + padding;
        paddingSegments->push_back(padding);
        while (currentMemoryBank > ADDRESS_SIZE)
            currentMemoryBank -= ADDRESS_SIZE;
        if (currentMemoryBank == ADDRESS_SIZE)
            currentMemoryBank = 0;
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

int main(int argc, char* argv[]) {   
    const char uaUrl[] = "opc.tcp://firing2:48010";//"opc.tcp://127.0.0.1:62541/milo";//"opc.tcp://milo.digitalpetri.com:62541/milo";//
    const UA_UInt16 numberOfIds = 7;
    const char id[numberOfIds][255] = { "Demo.Static.Scalar.CarExtras", "Demo.BoilerDemo.Boiler1.HeaterStatus", "Demo.Static.Scalar.OptionSet", "Demo.Static.Scalar.XmlElement", "Demo.Static.Scalar.Structure", "Demo.Static.Scalar.Union", "Demo.Static.Scalar.Structures.AnalogMeasurement" };//"ComplexTypes/CustomStructTypeVariable";//"ComplexTypes/CustomUnionTypeVariable";//"Person1";//"Demo.Static.Scalar.Priority";//"ComplexTypes/CustomEnumTypeVariable";//
    UA_NodeId nodeId;
    UA_Client* client = 0x0;
    UA_StatusCode retval = UA_STATUSCODE_BAD;
    UA_DataType userDataType;
    UA_String out;
    UA_Variant outValue;
    std::map<std::string, UA_Variant> structureMembers;
    std::map<UA_UInt32, std::string> dictionaries;
    std::map<UA_UInt32, std::vector<std::map<std::string, std::string>>> dataTypeAttributes;

    // pre-initialisation of the user data type
    userDataType.memSize = 0;
    userDataType.membersSize = 0;
    // connect to server
    client = UA_Client_new();
    UA_ClientConfig_setDefault(UA_Client_getConfig(client));
    retval = UA_Client_connect(client, uaUrl);
    if (retval != UA_STATUSCODE_GOOD) {
        UA_Client_delete(client);
        return EXIT_FAILURE;
    }
    // retrieve all dictionaries of OPC UA server
    if(getDictionaries(client, &dictionaries)) {    
        //printDictionaries(&dictionaries);
        // process all given node IDs
        for (UA_UInt16 i = 0; i < numberOfIds; i++) {
            nodeId = UA_NODEID_STRING_ALLOC(2, id[i]);
            // retrieve user data type and user data type attributes
            retval = getUserDataTypeAttributes(client, nodeId, &dictionaries, &userDataType, &dataTypeAttributes);
            if (retval == UA_STATUSCODE_UNCERTAIN) { // ENUMERATION
                UA_Variant_init(&outValue);
                retval = UA_Client_readValueAttribute(client, nodeId, &outValue);
                if (retval == UA_STATUSCODE_GOOD) {
                    UA_printEnum(&outValue, &userDataType, &dataTypeAttributes, &out);
                    printf("%.*s\n", (UA_UInt16)out.length, out.data);
                    UA_String_clear(&out);
                }
                UA_Variant_clear(&outValue);
            }
            else if (retval == UA_STATUSCODE_GOOD)  {
                //printUserDataTypeAttributes(&dataTypeAttributes, &userDataType);
                // reconnect with user data types
                UA_DataType* types;
                types = (UA_DataType*)UA_malloc(sizeof(UA_DataType));
                if (!types)
                    break;
                types[0] = userDataType;
                UA_DataTypeArray customDataTypes = { NULL, 1, types };
                UA_Client_getConfig(client)->customDataTypes = &customDataTypes;
                // read value
                UA_Variant_init(&outValue);
                retval = UA_Client_readValueAttribute(client, nodeId, &outValue);
                if (retval == UA_STATUSCODE_GOOD) {
                    UA_String_init(&out);
                    switch (userDataType.typeKind) {
                        case (UA_DATATYPEKIND_UNION): {
                            UA_printUnion(&outValue, &dataTypeAttributes, &out);
                            break;
                        }
                        case (UA_DATATYPEKIND_STRUCTURE): {
                            getStructureValues(client, outValue, &userDataType, &dataTypeAttributes, &structureMembers);
                            UA_printStructure(&outValue, &structureMembers, &out);
                            structureMembers.clear();
                            break;
                        }
                        case (UA_DATATYPEKIND_ENUM): {
                            break; // handled by getUserDataTypeAttributes() == UA_STATUSCODE_UNCERTAIN
                        }
                        default: {
                            UA_print(&outValue, &UA_TYPES[UA_TYPES_VARIANT], &out);
                        }
                    }
                    printf("%.*s\n", (UA_UInt16)out.length, out.data);
                    UA_String_clear(&out);
                }                
                UA_Variant_clear(&outValue);
                UA_NodeId_clear(&nodeId);
                UA_free(types);
            }
            else /*if (retval == UA_STATUSCODE_BAD)*/ { // no user data type was found
                UA_Variant_init(&outValue);
                retval = UA_Client_readValueAttribute(client, nodeId, &outValue);
                if (retval == UA_STATUSCODE_GOOD) {
                    UA_print(&outValue, &UA_TYPES[UA_TYPES_VARIANT], &out);
                    printf("%.*s\n", (UA_UInt16)out.length, out.data);
                    UA_String_clear(&out);
                }
                UA_Variant_clear(&outValue);
            }
        }
    } else {
        UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "failed to retrieve dictionaries");
    }
    // disconnect and clean up
    UA_Client_disconnect(client);
    UA_Client_delete(client);
    dataTypeAttributes.clear();
    dictionaries.clear();
    return EXIT_SUCCESS;
}
