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

#include <algorithm>
#include <cmath>
#include <map>
#include <string>
#include <vector>

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
// checks whether the xml node is a leaf (child)
UA_Boolean isLeaf(xmlNode* node);
// shortcut for warning message; the node ID is printed before the message
void printWarn(const char* message, UA_NodeId pvId);
void UA_printNodeClass(UA_NodeClass nodeClass, UA_String* out);

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
    if (xmlStrncasecmp((const xmlChar*)("opc:Bit"), (xmlChar*)text.data(), (UA_UInt32)text.length()) == 0)
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
    bReq.nodesToBrowse[0].browseDirection = UA_BROWSEDIRECTION_FORWARD;
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
    if (!node || !vec || vec->empty())
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
        xmlChar* value = xmlGetProp(node, BAD_CAST"EXO_BASEDATATYPE");
        if (value)
            map->insert(std::pair<std::string, std::string>(EXO_BASEDATATYPE, std::string((char*)value)));
        else
            map->insert(std::pair<std::string, std::string>(EXO_BASEDATATYPE, std::string("")));
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
static UA_StatusCode UA_printEnum(const void* pData, UA_DataType* userDataType, UA_String* output) {
    UA_UInt32 typeIdHash = UA_NodeId_hash(&userDataType->typeId);
    std::map<UA_UInt32, type_properties_t>::iterator typeProperty = getTypeProperty(typeIdHash);
    std::vector<std::map<std::string, std::string>>::iterator dataTypeAttributePropertySegment;
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
    retval |= UA_PrintContext_addName(&ctx, EXO_DATATYPE);
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
        dataTypeAttributePropertySegment = typeProperty->second.dataTypeAttributes.begin();
        if (dataTypeAttributePropertySegment == typeProperty->second.dataTypeAttributes.end())
            return UA_STATUSCODE_BAD;
        if (value < typeProperty->second.propertyMap.size()) {
            retval |= UA_PrintContext_addNewlineTabs(&ctx, ctx.depth);
            UA_PrintOutput* out = UA_PrintContext_addOutput(&ctx, typeProperty->second.propertyMap[value].length());
            if (!out)
                retval |= UA_STATUSCODE_BADOUTOFMEMORY;
            else
                memcpy(&out->data, typeProperty->second.propertyMap[value].c_str(), typeProperty->second.propertyMap[value].length());
        }
        else {
            retval |= UA_PrintContext_addString(&ctx, " (is not a number)");
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

// prints variant data type STRUCTURE to UA_String
static UA_StatusCode UA_printStructure(const void* pData, UA_DataType* userDataType, UA_String* output) {
    const UA_DataType* dataType;
    std::vector<UA_Variant> values;
    UA_Boolean* variantContent = 0x0;
    UA_Byte pos = 0;
    UA_Byte resultingBits = 0;
    UA_Byte* pValidBits = 0x0;
    UA_Byte* pValue = 0x0;
    UA_DataTypeMember* typeMember;
    UA_PrintContext ctx = UA_PrintContext();
    UA_StatusCode retval = UA_STATUSCODE_GOOD;
    UA_UInt32 nodeIdHash = UA_NodeId_hash(&userDataType->typeId);
    UA_Variant* p = (UA_Variant*)pData;
    UA_Variant* validBits = 0x0;
    UA_Variant* value = 0x0;
    uintptr_t ptrs = (uintptr_t)p->data;
    typeProperty = typePropertiesMap.find(nodeIdHash);
    if (typeProperty == typePropertiesMap.end()) {
        UA_print(pData, &UA_TYPES[userDataType->typeIndex], output);
        return UA_STATUSCODE_BAD;
    }
    if (ptrs && ((UA_ExtensionObject*)ptrs)->encoding == UA_EXTENSIONOBJECT_ENCODED_BYTESTRING) {
        for (UA_UInt16 i = 0; i < typeProperty->second.dataType.membersSize; i++) {
            typeMember = &typeProperty->second.dataType.members[i];
            dataType = &UA_TYPES[typeMember->memberTypeIndex];
            ptrs += typeMember->padding;
            if (!typeMember->isArray) {
                UA_Variant varVal;
                UA_Variant_init(&varVal);
                retval = UA_Variant_setScalarCopy(&varVal, (void*)ptrs, dataType);
                if (retval == UA_STATUSCODE_GOOD)
                    values.push_back(varVal);
                ptrs += dataType->memSize;
            }
        }
        if (values.size() == 2) {
            value = &values.at(0);
            validBits = &values.at(1);
            pValue = ((UA_ByteString*)value->data)->data;
            /*printf("\n%-25s: ", "value");
            binary_print(*pValue);
            printf(" (%d)\n", *pValue);*/
            pValidBits = ((UA_ByteString*)validBits->data)->data;
            /*printf("%-25s: ", "valid bits");
            binary_print(*pValidBits);
            printf(" (%d)\n", *pValidBits);*/
            resultingBits = *pValue & *pValidBits;
            /*printf("%-25s: ", "result");
            binary_print(resultingBits);
            printf(" (%d)\n", resultingBits);*/
            values.clear();
            for (std::string property : typeProperty->second.propertyMap) {
                UA_Variant varVal;
                UA_Variant_init(&varVal);
                variantContent = UA_Boolean_new();
                *variantContent = resultingBits & 0x01 << pos++;
                UA_Variant_setScalar(&varVal, variantContent, &UA_TYPES[UA_TYPES_BOOLEAN]);
                values.push_back(varVal);
            }
        }
    }
    ctx.depth = 0;
    TAILQ_INIT(&ctx.outputs);
    UA_String_init(output);
    if (!p->type)
        return UA_PrintContext_addString(&ctx, "NullVariant");
    retval |= UA_PrintContext_addString(&ctx, "{");
    ctx.depth++;

    retval |= UA_PrintContext_addNewlineTabs(&ctx, ctx.depth);
    retval |= UA_PrintContext_addName(&ctx, EXO_DATATYPE);
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
        for (UA_UInt16 i = 0; i < typeProperty->second.propertyMap.size(); i++) {
            retval |= UA_PrintContext_addNewlineTabs(&ctx, ctx.depth);
            const UA_DataType* type = &UA_TYPES[values.at(i).type->typeIndex];
            size_t len = typeProperty->second.propertyMap.at(i).length();
            UA_PrintOutput* out = UA_PrintContext_addOutput(&ctx, len);
            if (!out)
                retval |= UA_STATUSCODE_BADOUTOFMEMORY;
            else
                memcpy(&out->data, typeProperty->second.propertyMap.at(i).c_str(), len);
            retval |= UA_PrintContext_addString(&ctx, ": ");
            UA_print(values.at(i).data, type, &outString);
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
static UA_StatusCode UA_printUnion(const void* pData, UA_DataType* userDataType, UA_String* output) {
    UA_UInt32 typeIdHash = UA_NodeId_hash(&userDataType->typeId);
    std::map<UA_UInt32, type_properties_t>::iterator typeProperty = getTypeProperty(typeIdHash);
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
    retval |= UA_PrintContext_addName(&ctx, EXO_DATATYPE);
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
            std::vector<std::map<std::string, std::string>>::iterator dataTypeAttributePropertySegment;
            std::map<std::string, std::string>::iterator dataTypeAttributeProperty;
            for (dataTypeAttributePropertySegment = typeProperty->second.dataTypeAttributes.begin(); dataTypeAttributePropertySegment != typeProperty->second.dataTypeAttributes.end(); dataTypeAttributePropertySegment++) {
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
                UA_print((UA_Byte*)p->data + sizeof(UA_UInt32) + p->type->members[switchIndex - 1].padding, dataType, &outString);
                len = outString.length;
                out = UA_PrintContext_addOutput(&ctx, len);
                if (!out)
                    retval |= UA_STATUSCODE_BADOUTOFMEMORY;
                else
                    memcpy(&out->data, outString.data, outString.length);
                UA_String_clear(&outString);
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

// debug-print of data type property structure
void printTypeProperty(std::map<UA_UInt32, type_properties_t>::iterator typeProperty) {
    UA_String out;

    printf("%40.*s: ", (UA_UInt16)typeProperty->second.browseName.name.length, typeProperty->second.browseName.name.data);
    UA_print(&typeProperty->second.dataType.typeId, &UA_TYPES[UA_TYPES_NODEID], &out);
    printf("Node ID: %.*s (%u); ", (UA_UInt16)out.length, out.data, typeProperty->first);
    UA_String_clear(&out);
    UA_printNodeClass(typeProperty->second.nodeClass, &out);
    printf("Node Class: %.*s (%d); ", (UA_UInt16)out.length, out.data, (UA_UInt16)typeProperty->second.nodeClass);
    UA_String_clear(&out);
    UA_print(&typeProperty->second.dataType.binaryEncodingId, &UA_TYPES[UA_TYPES_NODEID], &out);
    printf("Binary Encoding ID: %.*s; ", (UA_UInt16)out.length, out.data);
    UA_String_clear(&out);
    UA_print(&typeProperty->second.dataTypeId, &UA_TYPES[UA_TYPES_NODEID], &out);
    printf("Type ID: %.*s; ", (UA_UInt16)out.length, out.data);
    UA_String_clear(&out);
    UA_print(&typeProperty->second.typeDefId, &UA_TYPES[UA_TYPES_NODEID], &out);
    printf("Type Definition ID: %.*s; ", (UA_UInt16)out.length, out.data);
    UA_String_clear(&out);
    UA_print(&typeProperty->second.propertyId, &UA_TYPES[UA_TYPES_NODEID], &out);
    printf("Property Node ID: %.*s; ", (UA_UInt16)out.length, out.data);
    UA_String_clear(&out);
    UA_print(&typeProperty->second.subTypeId, &UA_TYPES[UA_TYPES_NODEID], &out);
    printf("Subtype ID: %.*s; ", (UA_UInt16)out.length, out.data);
    UA_String_clear(&out);
    UA_print(&typeProperty->second.descriptionId, &UA_TYPES[UA_TYPES_NODEID], &out);
    printf("Description ID: %.*s\n", (UA_UInt16)out.length, out.data);
    UA_String_clear(&out);
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
    for (const UA_DataType* dataType : *structMemberTypes) {
        if (!dataType)
            continue;
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

// find the right data type property structure
std::map<UA_UInt32, type_properties_t>::iterator getTypeProperty(UA_UInt32 nodeIdHash) {
    std::map<UA_UInt32, type_properties_t>::iterator type_property;
    type_property = typePropertiesMap.find(nodeIdHash);
    if (type_property == typePropertiesMap.end()) {
        type_properties_t properties;
        memset(&properties, 0x0, sizeof(type_properties_t));
        UA_QualifiedName_init(&properties.browseName);
        typePropertiesMap.insert(std::pair<UA_UInt32, type_properties_t>(nodeIdHash, properties));
        type_property = typePropertiesMap.find(nodeIdHash);
    }
    return type_property;
}

// search data type tree recursively and collect node ids
void scanForTypeIds(UA_BrowseResponse* bResp, std::map<UA_UInt32, UA_NodeId>* ids) {
    UA_BrowseResult bRes;
    UA_ReferenceDescription rDesc;
    for (size_t i = 0; i < bResp->resultsSize; ++i) {
        bRes = bResp->results[i];
        for (size_t j = 0; j < bRes.referencesSize; ++j) {
            rDesc = bRes.references[j];
            ids->insert(std::pair<UA_UInt32, UA_NodeId>(UA_NodeId_hash(&rDesc.nodeId.nodeId), rDesc.nodeId.nodeId));
        }
    }
}

// translate node class to string
void UA_printNodeClass(UA_NodeClass nodeClass, UA_String* out) {
    switch (nodeClass) {
    case UA_NODECLASS_UNSPECIFIED: *out = UA_STRING_ALLOC("UNSPECIFIED"); break;
    case  UA_NODECLASS_OBJECT: *out = UA_STRING_ALLOC("OBJECT"); break;
    case UA_NODECLASS_VARIABLE: *out = UA_STRING_ALLOC("VARIABLE"); break;
    case UA_NODECLASS_METHOD: *out = UA_STRING_ALLOC("METHOD"); break;
    case UA_NODECLASS_OBJECTTYPE: *out = UA_STRING_ALLOC("OBJECTTYPE"); break;
    case UA_NODECLASS_VARIABLETYPE: *out = UA_STRING_ALLOC("VARIABLETYPE"); break;
    case UA_NODECLASS_REFERENCETYPE: *out = UA_STRING_ALLOC("REFERENCETYPE"); break;
    case  UA_NODECLASS_DATATYPE: *out = UA_STRING_ALLOC("DATATYPE"); break;
    case UA_NODECLASS_VIEW: *out = UA_STRING_ALLOC("VIEW"); break;
    case __UA_NODECLASS_FORCE32BIT: *out = UA_STRING_ALLOC("FORCE32BIT"); break;
    default: *out = UA_STRING_ALLOC("UNKNOWN"); break;
    }
}

void initializeUserDataTypeIds(UA_Client* client) {
    std::map<std::string, std::string>::iterator dataTypeAttributeProperty;
    std::map<UA_UInt32, std::string> dictionaries;
    std::map<UA_UInt32, type_properties_t>::iterator typeProperty;
    std::map<UA_UInt32, UA_NodeId> ids;
    std::map<UA_UInt32, xmlDocPtr> xmlDocMap;
    std::map<UA_UInt32, xmlDocPtr>::iterator xmlDocNode;
    std::vector<const UA_DataType*> dataTypeList;
    std::vector<std::map<std::string, std::string>>::iterator dataTypeAttributePropertySegment;
    std::vector<UA_Byte> paddings;
    std::vector<UA_Byte>::iterator padding;
    std::vector<UA_UInt32> knownIds;
    std::vector<UA_UInt32>::iterator knownId;
    UA_BrowseResponse bResp;
    UA_StatusCode retval = UA_STATUSCODE_BAD;
    UA_String out;
    UA_DataType* dataType = 0x0;
    const UA_DataType* currentType = 0x0;
    UA_UInt32 typeIdHash = 0;
    type_properties_t* typeProperties;
    xmlChar* parentName = NULL;
    xmlNode* attributeNode = NULL;
    numberOfUserDataTypes = 0;

    // retrieve all dictionaries of OPC UA server 
    if (getDictionaries(client, &dictionaries)) {
        UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "retrieve dictionaries was successful");
    }
    else {
        UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "retrieve dictionaries failed");
        return;
    }
    // build a document map of dictionary entries
    getXmlDocMap(&dictionaries, &xmlDocMap);
    // collect known basic data types
    ids.insert(std::pair<UA_UInt32, UA_NodeId>(UA_NodeId_hash(&NS0ID_BASEDATATYPE), NS0ID_BASEDATATYPE));
    while (knownIds.size() != ids.size()) {
        for (const std::pair<UA_UInt32, UA_NodeId>& id : ids) {
            if ((knownId = std::find(knownIds.begin(), knownIds.end(), id.first)) != knownIds.end())
                continue;
            typeIdHash = UA_NodeId_hash(&id.second);
            knownIds.push_back(typeIdHash);
            retval = browseNodeId(client, id.second, &bResp);
            if (retval != UA_STATUSCODE_GOOD)
                continue;
            if (id.second.namespaceIndex > 0 || id.second.identifier.numeric >= UA_TYPES_COUNT) {
                typeProperty = getTypeProperty(typeIdHash);
                typeProperties = &typeProperty->second;
                typeProperties->dataType.typeId = id.second;
                for (UA_UInt32 i = 0; i < bResp.results[0].referencesSize; i++)
                    typeProperties->descriptionList.push_back(bResp.results[0].references[i]);
            }
            scanForTypeIds(&bResp, &ids);
        }
    }
    // collect properties of data types
    for (typeProperty = typePropertiesMap.begin(); typeProperty != typePropertiesMap.end();) {
        typeProperties = &typeProperty->second;
        dataType = &typeProperties->dataType;
        retval = UA_Client_readNodeClassAttribute(client, dataType->typeId, &typeProperties->nodeClass);
        if (typeProperties->nodeClass != UA_NODECLASS_DATATYPE) {
            typeProperty++;
            continue;
        }
        UA_NodeId_copy(&dataType->typeId, &typeProperties->dataTypeId);
        retval = UA_Client_readBrowseNameAttribute(client, dataType->typeId, &typeProperties->browseName);
#ifdef UA_ENABLE_TYPEDESCRIPTION
        dataType->typeName = (char*)malloc((typeProperties->browseName.name.length + 1) * sizeof(char));
        if (dataType->typeName) {
            strncpy((char*)dataType->typeName, (char*)typeProperties->browseName.name.data, typeProperties->browseName.name.length);
            ((char*)dataType->typeName)[typeProperties->browseName.name.length] = 0x0;
        }
#endif 
        for (auto& description : typeProperties->descriptionList) {
            if (UA_NodeId_equal(&description.referenceTypeId, &NS0ID_HASTYPEDEFINITION)) {
                typeProperties->typeDefId = description.nodeId.nodeId;
            }
            else if (UA_NodeId_equal(&description.referenceTypeId, &NS0ID_HASSUBTYPE)) {
                typeProperty->second.subTypeId = description.nodeId.nodeId;
            }
            else if (UA_NodeId_equal(&description.referenceTypeId, &NS0ID_HASPROPERTY)) {
                typeProperties->propertyId = description.nodeId.nodeId;
                UA_Variant outValue;
                if (UA_Client_readValueAttribute(client, typeProperties->propertyId, &outValue) == UA_STATUSCODE_GOOD && !UA_Variant_isScalar(&outValue)) {
                    if (outValue.type->typeId.namespaceIndex == 0 && outValue.type->typeId.identifierType == UA_NODEIDTYPE_NUMERIC && outValue.type->typeId.identifier.numeric == UA_NS0ID_LOCALIZEDTEXT) {
                        UA_LocalizedText* data = (UA_LocalizedText*)outValue.data;
                        uintptr_t target = (uintptr_t)outValue.arrayDimensions;
                        for (UA_UInt32 i = 0; i < (UA_UInt32)outValue.arrayLength; i++) {
                            typeProperties->propertyMap.push_back(std::string((char*)data[i].text.data, data[i].text.length));
                        }
                    }
                }
            }
            else if (UA_NodeId_equal(&description.referenceTypeId, &NS0ID_HASENCODING)) {
                typeProperties->dataType.binaryEncodingId = description.nodeId.nodeId;
            }
            else if (UA_NodeId_equal(&description.referenceTypeId, &NS0ID_HASDESCRIPTION)) {
                typeProperties->descriptionId = description.nodeId.nodeId;
            }
            else {
                UA_print(&description.referenceTypeId, &UA_TYPES[UA_TYPES_NODEID], &out);
                printf("Node ID: %.*s (%u)\t", (UA_UInt16)out.length, out.data, typeProperty->first);
                UA_String_clear(&out);
            }
        }
        // add data type properties from dictionary
        for (xmlDocNode = xmlDocMap.begin(); xmlDocNode != xmlDocMap.end(); ++xmlDocNode) {
            xmlNode* root_element = xmlDocGetRootElement(xmlDocNode->second);
            if (findBrowseName(root_element, typeProperties->browseName.name, &attributeNode)) {
                std::map<std::string, std::string> map;
                xmlChar* parentName = xmlGetProp(attributeNode, (const xmlChar*)("Name"));
                if (parentName) {
                    map.insert(std::pair<std::string, std::string>(std::string(EXO_DATANAME), std::string((char*)parentName)));
                    typeProperties->dataTypeAttributes.push_back(map);
                    collectProperties(attributeNode, &typeProperties->dataTypeAttributes, UA_TRUE);
                    collectProperties(attributeNode->children, &typeProperties->dataTypeAttributes, UA_FALSE);
                }
                retval = UA_STATUSCODE_GOOD;
                break;
            }
        }
        if (typeProperties->dataTypeAttributes.empty()) {
            typeProperty++;
            continue;
        }
        dataTypeAttributePropertySegment = typeProperties->dataTypeAttributes.begin();
        dataTypeAttributeProperty = dataTypeAttributePropertySegment->find(EXO_DATATYPE_NS0ID);
        if (dataTypeAttributeProperty != dataTypeAttributePropertySegment->end()) {
            if (std::stoul(dataTypeAttributeProperty->second) == UA_NS0ID_ENUMDEFINITION) {
                typeProperties->dataType.typeKind = UA_DATATYPEKIND_ENUM;
                typeProperties->dataType.typeIndex = UA_TYPES_EXTENSIONOBJECT;
            }
            else if (std::stoul(dataTypeAttributeProperty->second) == UA_NS0ID_STRUCTUREDEFINITION) {
                typeProperties->dataType.typeKind = UA_DATATYPEKIND_STRUCTURE;
                typeProperties->dataType.typeIndex = UA_TYPES_EXTENSIONOBJECT;
            }
            else {
                UA_LOG_WARNING(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "unknown data type kind %s", dataTypeAttributeProperty->second.c_str());
            }
        }
        for (auto& dataTypeAttribute : typeProperties->dataTypeAttributes) {
            dataTypeAttributeProperty = dataTypeAttribute.find("SwitchField");
            if (dataTypeAttributeProperty != dataTypeAttribute.end())
                typeProperties->dataType.typeKind = UA_DATATYPEKIND_UNION;
            dataTypeAttributeProperty = dataTypeAttribute.find("TypeName");
            if (dataTypeAttributeProperty != dataTypeAttribute.end()) {
                currentType = parseDataType(dataTypeAttributeProperty->second);
                if (currentType)
                    dataTypeList.push_back(currentType);
                else {
                    UA_LOG_WARNING(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "unknown data type %s", dataTypeAttributeProperty->second.c_str());
                    dataTypeList.clear();
                    break;
                }
            }
        }
        if (dataTypeList.size()) {
            if (typeProperties->dataType.typeKind == UA_DATATYPEKIND_UNION) {
                const UA_DataType* switchFieldType = 0x0;
                const UA_DataType* fieldType = 0x0;
                switchFieldType = dataTypeList.at(0);
                for (UA_UInt32 i = 1; i < dataTypeList.size(); i++) {
                    if (!fieldType || fieldType->memSize < dataTypeList[i]->memSize)
                        fieldType = dataTypeList[i];
                }
                dataTypeList.clear();
                dataTypeList.push_back(switchFieldType);
                dataTypeList.push_back(fieldType);
            }
            typeProperties->dataType.membersSize = dataTypeList.size();
            paddings.clear();
            typeProperties->dataType.memSize = calc_struct_padding(&dataTypeList, &paddings);
            typeProperties->dataType.members = (UA_DataTypeMember*)UA_malloc(typeProperties->dataType.membersSize * sizeof(UA_DataTypeMember));
            if (typeProperties->dataType.members) {
                memset(typeProperties->dataType.members, 0x0, typeProperties->dataType.membersSize * sizeof(UA_DataTypeMember));
                typeProperties->dataType.pointerFree = UA_TRUE;
                padding = paddings.begin();
                for (UA_UInt32 i = 0; i < typeProperties->dataType.membersSize && i < dataTypeList.size() && padding != paddings.end(); i++) {
                    if (!dataTypeList[i]) {
                        padding++;
                        continue;
                    }
                    typeProperties->dataType.members[i].memberTypeIndex = dataTypeList[i]->typeIndex;
                    typeProperties->dataType.members[i].isArray = UA_FALSE;
                    typeProperties->dataType.members[i].isOptional = UA_FALSE;
                    if (dataTypeList[i]->typeIndex > UA_TYPES_DOUBLE)
                        typeProperties->dataType.pointerFree;
                    typeProperties->dataType.members[i].namespaceZero = dataTypeList[i]->typeId.namespaceIndex == 0;
                    typeProperties->dataType.members[i].padding = *padding;
#ifdef UA_ENABLE_TYPEDESCRIPTION
                    size_t length = strlen(dataTypeList[i]->typeName);
                    typeProperties->dataType.members[i].memberName = (char*)UA_malloc((length + 1) * sizeof(char));
                    strncpy((char*)typeProperties->dataType.members[i].memberName, dataTypeList[i]->typeName, length);
                    ((char*)typeProperties->dataType.members[i].memberName)[length] = 0x0;
#endif
                    padding++;
                }
            }
            else {
                UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "cannot allocate memory for %.*s", (UA_UInt16)typeProperties->browseName.name.length, typeProperties->browseName.name.data);
            }
        }
        else {
            typeProperty->second.dataType.memSize = 0;
        }
        printTypeProperty(typeProperty);
        dataTypeList.clear();
        typeProperty++;
    }
    // count valid user data types
    typeProperty = typePropertiesMap.begin();
    while (typeProperty != typePropertiesMap.end()) {
        if (typeProperty->second.dataType.memSize)
            numberOfUserDataTypes++;
        typeProperty++;
    }
    // build user data types
    customDataTypes = (UA_DataTypeArray*)UA_malloc(numberOfUserDataTypes * sizeof(UA_DataTypeArray));
    if (!customDataTypes)
        return;
    memset(customDataTypes, 0x0, numberOfUserDataTypes * sizeof(UA_DataTypeArray));
    typeProperty = typePropertiesMap.begin();
    UA_UInt32 j = 0;
    for (size_t i = 0; i < typePropertiesMap.size(); i++) {
        if (!typeProperty->second.dataType.memSize) {
            typeProperty++;
            continue;
        }
        //printTypeProperty(typeProperty);
        customDataTypes[j].types = &typeProperty->second.dataType;
        *(size_t*)&(customDataTypes[j]).typesSize = 1;
        if ((i + 1) < typePropertiesMap.size() && (j + 1) < numberOfUserDataTypes)
            customDataTypes[j].next = &customDataTypes[j + 1];
        else
            customDataTypes[j].next = 0x0;
        j++;
        typeProperty++;
    }
    // configure client session with user data types
    UA_Client_getConfig(client)->customDataTypes = customDataTypes;
}

int main(int argc, char* argv[]) {
    const char* uaUrl = "opc.tcp://firing2:48010";//"opc.tcp://127.0.0.1:62541/milo";//"opc.tcp://milo.digitalpetri.com:62541/milo";//
    const UA_UInt16 numberOfIds = 8;
    const char id[numberOfIds][255] = { "Demo.Static.Scalar.CarExtras" , "Demo.BoilerDemo.Boiler1.HeaterStatus", "Demo.Static.Scalar.OptionSet", "Demo.Static.Scalar.XmlElement", "Demo.Static.Scalar.Structure", "Demo.Static.Scalar.Union", "Demo.Static.Scalar.Structures.AnalogMeasurement", "Person1" };//"ComplexTypes/CustomStructTypeVariable";//"ComplexTypes/CustomUnionTypeVariable";//"Person1";//"Demo.Static.Scalar.Priority";//"ComplexTypes/CustomEnumTypeVariable";//
    UA_NodeId nodeId;
    UA_Client* client = 0x0;
    UA_StatusCode retval = UA_STATUSCODE_BAD;
    UA_String out;
    UA_Variant outValue;

    if (argc > 1) uaUrl = argv[1];

    // pre-initialisation of the user data type
    // connect to server
    client = UA_Client_new();
    UA_ClientConfig_setDefault(UA_Client_getConfig(client));
    retval = UA_Client_connect(client, uaUrl);
    if (retval != UA_STATUSCODE_GOOD) {
        UA_Client_delete(client);
        return EXIT_FAILURE;
    }
    initializeUserDataTypeIds(client);
    for (UA_UInt16 i = 0; i < numberOfIds; i++) {
        nodeId = UA_NODEID_STRING_ALLOC(2, id[i]);
        // print node ID
        UA_print(&nodeId, &UA_TYPES[UA_TYPES_NODEID], &out);
        printf("%.*s: ", (UA_UInt16)out.length, out.data);
        UA_String_clear(&out);
        // read value
        UA_Variant_init(&outValue);
        retval = UA_Client_readValueAttribute(client, nodeId, &outValue);
        if (retval == UA_STATUSCODE_GOOD) {
            // get printable value
            typeProperty = typePropertiesMap.find(UA_NodeId_hash(&outValue.type->typeId));
            if (typeProperty == typePropertiesMap.end()) {
                UA_print(&outValue, &UA_TYPES[UA_TYPES_VARIANT], &out);
                printf("%.*s\n", (UA_UInt16)out.length, out.data);
                UA_String_clear(&out);
                UA_Variant_clear(&outValue);
                UA_NodeId_clear(&nodeId);
                continue;
            }
            switch (typeProperty->second.dataType.typeKind) {
            case (UA_DATATYPEKIND_UNION): {
                UA_printUnion(&outValue, &typeProperty->second.dataType, &out);
                break;
            }
            case (UA_DATATYPEKIND_STRUCTURE): {
                UA_printStructure(&outValue, &typeProperty->second.dataType, &out);
                break;
            }
            case (UA_DATATYPEKIND_ENUM): {
                UA_printEnum(&outValue, &typeProperty->second.dataType, &out);
                break;
            }
            default: {
                UA_print(&outValue, &UA_TYPES[UA_TYPES_VARIANT], &out);
            }
            }
            // print value
            printf("%.*s\n", (UA_UInt16)out.length, out.data);
            UA_String_clear(&out);
            UA_Variant_clear(&outValue);
        }
        else {
            UA_print(&nodeId, &UA_TYPES[UA_TYPES_NODEID], &out);
            UA_LOG_WARNING(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "could not read %.*s", (UA_UInt16)out.length, out.data);
            UA_String_clear(&out);
        }
        UA_NodeId_clear(&nodeId);
    }
    if (customDataTypes)
        UA_free(customDataTypes);
    UA_Client_disconnect(client);
    UA_Client_delete(client);
    return EXIT_SUCCESS;
}