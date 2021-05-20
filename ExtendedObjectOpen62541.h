/*************************************************************************\
* Copyright (c) 2021 HZB.
* Author: Carsten Winkler carsten.winkler@helmholtz-berlin.de
\*************************************************************************/

#pragma once
#define EXO_USER_DICIONARIES_ONLY // optimised dictionary processing
#define ADDRESS_SIZE 8 // used for padding calculation
#define MEMORY_BANK_SIZE 4 // used for padding calculation
#define UA_NAMESPACEOFFSET 1000 // the name space offset is used to read data types with the same name space index from different dictionaries
#define EXO_BASEDATATYPE "BaseType"
#define EXO_DATANAME "Name"
#define EXO_DATATYPE "DataType"
#define EXO_DATATYPE_NS0ID "DataTypeNS0ID"
typedef struct {
	std::vector<std::map<std::string, std::string>> dataTypeAttributes;
	std::vector<std::string> propertyMap;
	std::vector<UA_ReferenceDescription> descriptionList;
	UA_DataType dataType;
	UA_NodeClass nodeClass;
	UA_NodeId dataTypeId;
	UA_NodeId descriptionId;
	UA_NodeId propertyId;
	UA_NodeId subTypeId;
	UA_NodeId typeDefId;
	UA_QualifiedName browseName;
} type_properties_t;
const UA_NodeId NS0ID_BASEDATATYPE = UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATATYPE);
const UA_NodeId NS0ID_HASDESCRIPTION = UA_NODEID_NUMERIC(0, UA_NS0ID_HASDESCRIPTION);
const UA_NodeId NS0ID_HASENCODING = UA_NODEID_NUMERIC(0, UA_NS0ID_HASENCODING);
const UA_NodeId NS0ID_HASPROPERTY = UA_NODEID_NUMERIC(0, UA_NS0ID_HASPROPERTY);
const UA_NodeId NS0ID_HASSUBTYPE = UA_NODEID_NUMERIC(0, UA_NS0ID_HASSUBTYPE);
const UA_NodeId NS0ID_HASTYPEDEFINITION = UA_NODEID_NUMERIC(0, UA_NS0ID_HASTYPEDEFINITION);
std::map<UA_UInt32, type_properties_t> typePropertiesMap;
std::map<UA_UInt32, type_properties_t>::iterator typeProperty;
UA_DataTypeArray* customDataTypes = NULL;
UA_UInt32 numberOfUserDataTypes;

void binary_print(UA_Byte value);
UA_Int16 getDictionaries(UA_Client* client, std::map<UA_UInt32, std::string>* dictionaries);
std::map<UA_UInt32, type_properties_t>::iterator getTypeProperty(UA_UInt32 nodeIdHash);
void printDictionaries(std::map<UA_UInt32, std::string>* dictionaries);
static UA_StatusCode UA_printEnum(const void* pData, UA_DataType* userDataType, UA_String* output);
static UA_StatusCode UA_printStructure(const void* pData, UA_DataType* userDataType, UA_String* output);
static UA_StatusCode UA_printUnion(const void* pData, UA_DataType* userDataType, UA_String* output);
