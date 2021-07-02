/*************************************************************************\
* Copyright (c) 2021 HZB.
* Author: Carsten Winkler carsten.winkler@helmholtz-berlin.de
\*************************************************************************/

#pragma once
// context information for custom data type
typedef struct {
	// If you change this structure, DO NOT forget to also change customTypePropertiesInit()!
	UA_DataType dataType;
	UA_NodeId subTypeOfId;
	std::vector<UA_EnumValueType> enumValueSet;
	std::vector<UA_StructureDefinition> structureDefinition;
} customTypeProperties_t;
typedef std::map<std::string, customTypeProperties_t*>::iterator nameTypePropIt_t;
typedef std::map<const UA_UInt32, customTypeProperties_t>::iterator typePropIt_t;

static const UA_NodeId NS0ID_BASEDATATYPE = UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATATYPE);
static const UA_NodeId NS0ID_ENUMDEFINITION_ENCODING_DEFAULTBINARY = UA_NODEID_NUMERIC(0, UA_NS0ID_ENUMDEFINITION_ENCODING_DEFAULTBINARY);
static const UA_NodeId NS0ID_ENUMERATION = UA_NODEID_NUMERIC(0, UA_NS0ID_ENUMERATION);
static const UA_NodeId NS0ID_HASENCODING = UA_NODEID_NUMERIC(0, UA_NS0ID_HASENCODING);
static const UA_NodeId NS0ID_HASPROPERTY = UA_NODEID_NUMERIC(0, UA_NS0ID_HASPROPERTY);
static const UA_NodeId NS0ID_HASSUBTYPE = UA_NODEID_NUMERIC(0, UA_NS0ID_HASSUBTYPE);
static const UA_NodeId NS0ID_OPTIONSET = UA_NODEID_NUMERIC(0, UA_NS0ID_OPTIONSET);
static const UA_NodeId NS0ID_STRUCTURE = UA_NODEID_NUMERIC(0, UA_NS0ID_STRUCTURE);
static const UA_NodeId NS0ID_UNION = UA_NODEID_NUMERIC(0, UA_NS0ID_UNION);

static std::map<std::string, customTypeProperties_t*> dataTypeNameMap;
static std::map<UA_UInt32, customTypeProperties_t> dataTypeMap;
static UA_DataTypeArray* customDataTypes;
UA_StatusCode getDictionaries(UA_Client* client, std::map<UA_UInt32, std::string>* dictionaries);
UA_StatusCode initializeCustomDataTypes(UA_Client* client);
UA_StatusCode UA_PrintCustomDataTypeMap(UA_String* output);
UA_StatusCode UA_PrintDataType(const UA_DataType* dataType, UA_String* output);
UA_StatusCode UA_PrintDataTypeMember(UA_DataTypeMember* dataTypeMember, UA_String* output);
UA_StatusCode UA_PrintDictionaries(UA_Client* client, UA_String* output);
UA_StatusCode UA_PrintEnum(const UA_Variant* data, customTypeProperties_t* customTypeProperties, UA_String* output);
UA_StatusCode UA_PrintStructure(const UA_Variant* data, UA_String* output);
UA_StatusCode UA_PrintUnion(const UA_Variant* data, UA_String* output);
UA_StatusCode UA_PrintValue(UA_Client* client, UA_NodeId nodeId, UA_Variant* data, UA_String* output);
static UA_UInt32 numberOfCustomDataTypes;
void customTypePropertiesInit(customTypeProperties_t* customTypeProperties, const UA_NodeId* customDataTypeId);
