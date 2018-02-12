//======================================================================================================================
// Copyright (C) CSEE - Transport 1999             Project  : SMS
// ---------------------------------------------------------------------------------------------------------------------
// File        : SmsFilePaMgr.cpp                           Creation : 21/06/99
// Author      : C.VERGNES
// Module      : UPDATEPA
// ---------------------------------------------------------------------------------------------------------------------
// Description : Class implementation
//======================================================================================================================

// ILOG Server
#include <ilserver\mvcomp.h>							// IlsMvComponent

// RogueWave
#include <rw\cstring.h>									// RWCString

// SMS Update
#include <UpdatePa\SmsFilePaMgr.h>						// SmsFilePaMgr
#include <UpdatePa\SmsUpdatePa.h>						// SmsUpdatePa
#include <UpdatePa\SmsParametersPa.h>					// SmsParametersPa

// SMS Shared
#include <SmsTrace\SmsTrace.h>							// SmsTrace
#include <SmsDefineShared\SmsPaDefine.h>				// PA_SECTION_...
#include <SmsDefineSmss\SmsUpdateDefine.h>				// SMS_TRACE_...
#include <ReportEvent\SmsReportEvent.h>					// SmsReportEvent
#include <Command\SmsCommandPaId.h>						// PA_CMD_...
#include <CmdMgr\SmsCommandManager.h>				    // SmsCommandManager

// Sms Util
#include <SmsUtil/SmsFileAnalyser.h>					// SmsFileAnalyser

// Standards
#include <istream.h>									// istream

//----------------------------------------------------------------------------------------------------------------------
//#BEGIN
//#CLASS SmsFilePaMgr
//#COMMENT This Pa file manager is used to analyse Pa files received from Pa equipment.
//#END
//----------------------------------------------------------------------------------------------------------------------
long SmsFilePaMgr::OpenSection = PA_SECTION_UNKNOWN ;
long SmsFilePaMgr::LastPredefMsgLineType = PA_MSG_LINE_TYPE_UNKNOWN ;

//----------------------------------------------------------------------------------------------------------------------
//#BEGIN
//#METHOD bool SmsFilePaMgr::ReadZoneConfig( const char *fileName )
//#COMMENT This method is used to read zone configuration file. A command is sent to server for each zone described in
// file. These commands are sent in a 'transaction' : a first command is sent to start transaction, then each zone
// configuration commands are sent and a last command is sent to close transaction. The aim of this transaction is to
// know which zones are no more described in zone configuration file and must be deleted.
//#END
//----------------------------------------------------------------------------------------------------------------------
bool SmsFilePaMgr::ReadZoneConfig( const char *fileName )
{
	SmsTrace trace( "SmsFilePaMgr::ReadZoneConfig", SMS_TRACE_PA_FTP, 0x06 ) ;

	SmsParametersPa* param = SmsUpdatePa::Get()->getParameters() ;

	// Open zone configuration file
    ifstream input( fileName ) ;
	if ( !input )
	{
		trace.printn( "*** Cannot open file %s ***", fileName ) ;
		return false ;
	}

	// Find zone configuration part
	if ( !SmsFileAnalyser::FindField( input, "[ZoneInfo]" ) )
	{
		trace.printn( "*** Cannot find zone info field ***" ) ;
		return false ;
	}

	// Begin of zone configuration transaction
	SendCommand( PA_CMD_BEGIN_ZONE_CONFIG ) ;

	// Retrieve zone description line format
	RWCString format = param->getZoneLineFormat() ;

	int logicalAddress ;
	char physicalAddressLine[256] ;

	RWCString line ;
	bool zoneDescriptionEnd = false ;

	// Start loop to analyse each lines
	// Zone description ends when line does not start with 'Zone'
	while ( (!zoneDescriptionEnd)||( line.readLine( input ) ) )
	{
		memset( physicalAddressLine, 0, sizeof( physicalAddressLine ) ) ;
		logicalAddress = 0 ;

		// Analyse zone description line
		int resSscanf = sscanf( line.data(), format.data(), &logicalAddress, physicalAddressLine ) ;

		if ( resSscanf == 2 )
		// All line parameters have been extracted
		{
			trace.printn( "Analysed line : %s", line.data() ) ;

			// Find zone name
			SmsDBPaZoneRef zoneRef ;
			if ( !param->getZoneMap()->getUkKeyPaZone( SmsDBPaZone::toUkKeyPaZoneKey( logicalAddress ), zoneRef ) )
			{
				trace.printn( "*** Zone %d not found in mapping ***", logicalAddress ) ;
				continue ;
			}

			int physicalAddress ;
			RWCString mask ;
			// Build mask
			if ( !BuildMask( physicalAddressLine, mask, physicalAddress ) )
			{
				trace.printn( "*** Invalid physical address line %s ***", physicalAddressLine ) ;
				continue ;
			}

			// Create and send command
			SendCommand( PA_CMD_ZONE_CREATION, zoneRef->getEquipmentId().data(), mask.data(), physicalAddress ) ;

			trace.printn( "Create zone %s : <mask>%s, <physicalAddress>%d",
				zoneRef->getEquipmentId().data(), mask.data(), physicalAddress ) ;
		}
		else
		{
			if ( resSscanf == 1 )
				// Only 1 line parameter could be extracted
				trace.printn( "*** Invalid zone description line %s ***", line.data() ) ;
			else
				// No parameter could be extracted (means line does not start with 'Zone')
				zoneDescriptionEnd = true ;
		}
	}//while

	// End of zone configuration transaction
	SendCommand( PA_CMD_END_ZONE_CONFIG ) ;

	return true ;
}

//----------------------------------------------------------------------------------------------------------------------
//#BEGIN
//#METHOD bool SmsFilePaMgr::ReadMsgConfig( const char *fileName )
//#COMMENT This method is used to read Pa database file. This file is devided into three part : message types
// description part, message variables description part and predefined messages part. A command is sent to server for
// each message types, message variables or predefined messages described in file. These commands are sent in three
// different 'transactions'. For each 'transaction', a first command is sent to start transaction, then commands are
// sent and a last command is sent to close transaction. The aim of these transactions is to know which message types,
// message variables and predefined messages are no more described in Pa database file and must be deleted.
//#END
//----------------------------------------------------------------------------------------------------------------------
bool SmsFilePaMgr::ReadMsgConfig( const char *fileName )
{
	SmsTrace trace( "SmsFilePaMgr::ReadMsgConfig", SMS_TRACE_PA_FTP, 0x06 ) ;

	SmsUpdatePa* updatePa = SmsUpdatePa::Get() ;

	// Open Pa database file
    ifstream input( fileName ) ;
	if ( !input )
	{
		trace.printn( "*** Cannot open file %s ***", fileName ) ;
		return false ;
	}

	// Begin of message types configuration transaction
	SendCommand( PA_CMD_BEGIN_MSG_TYPE_CONFIG ) ;
	OpenSection = PA_SECTION_MSG_TYPE ;
	LastPredefMsgLineType = PA_MSG_LINE_WITHOUT_VAR ;

	RWCString line ;
	// Start loop to analyse each lines
	while ( line.readLine( input ) )
	{
		trace.printn( "Analysed line : %s", line.data() ) ;

		// Analyse line
		if ( !AnalyseMsgConfigLine( line, OpenSection ) )
		{
			trace.printn( "*** Syntax error ***" ) ;
		}

	}//while

	// Close the open section
	switch ( OpenSection )
	{
		case PA_SECTION_MSG_TYPE :
		{
			// End of message types configuration
			SendCommand( PA_CMD_END_MSG_TYPE_CONFIG ) ;
		}
		break ;
		case PA_SECTION_MSG_VAR :
		{
			// End of message variables configuration
			SendCommand( PA_CMD_END_MSG_VAR_CONFIG ) ;
		}
		break ;
		case PA_SECTION_PREDEF_MSG :
		{
			// End of predefined messages configuration
			SendCommand( PA_CMD_END_PREDEF_MSG_CONFIG ) ;
		}
		break ;
		default :
		{
			SmsReportEvent::Error(LOC(967),CY_PA,"Invalid file section", OpenSection);
			return false ;
		}
	}

	return true ;
}

//----------------------------------------------------------------------------------------------------------------------
//#BEGIN
//#METHOD bool SmsFilePaMgr::ReadInstantMsgConfig( const char *fileName )
//#COMMENT This method is used to read instant message file. A command is sent to server for each instant message
// described in file. These commands are sent in a 'transaction' : a first command is sent to start transaction, then
// each instant message commands are sent and a last command is sent to close transaction. The aim of this transaction
// is to know which instant messages are no more described in instant message file and must be deleted.
//#END
//----------------------------------------------------------------------------------------------------------------------
bool SmsFilePaMgr::ReadInstantMsgConfig( const char *fileName )
{
	SmsTrace trace( "SmsFilePaMgr::ReadInstantMsgConfig", SMS_TRACE_PA_FTP, 0x06 ) ;

	SmsUpdatePa* updatePa = SmsUpdatePa::Get() ;

	// Open instant message file
    ifstream input( fileName ) ;
	if ( !input )
	{
		trace.printn( "*** Cannot open file %s ***", fileName ) ;
		return false ;
	}

	// Begin of instant messages configuration
	SendCommand( PA_CMD_BEGIN_INSTANT_MSG_CONFIG ) ;

	RWCString line ;
	RWCString format = updatePa->getParameters()->getMsgWithoutVarLineFormat() ;
	char msgType[256] ;
	int msgId ;
	char chineseOrEnglish[256] ;
	char msgText[256] ;

	// Start loop to analyse each lines
	while ( line.readLine( input ) )
	{
		trace.printn( "Analysed line : %s", line.data() ) ;

		memset( msgType, 0, sizeof( msgType ) ) ;
		memset( chineseOrEnglish, 0, sizeof( chineseOrEnglish ) ) ;
		memset( msgText, 0, sizeof( msgText ) ) ;
		msgId = 0 ;

		int resSscanf = sscanf( line.data(), format.data(), msgType, &msgId, chineseOrEnglish, msgText ) ;
		if ( resSscanf < 4 )
		{
			trace.printn( "*** Syntax error : only %d parameters found in instant message line", resSscanf ) ;
			trace.printn( "\t<msgType> %s, <msgId> %d, <C/E> %s, <msgText> %s ***",
				msgType, msgId, chineseOrEnglish, msgText ) ;
		}
		else
		{
			// Chinese line are ignored
			if ( !strcmp( chineseOrEnglish, "C" ) )
				trace.printn( "Chinese message config line" ) ;
			else
			{
				RWCString text = msgText ;
				RWCString type = msgType ;
				SmsFileAnalyser::ExtractSpaces( type ) ;

				// Extract message title from its text
				RWCString msgName = ReadTitle( text ) ;

				// Create and send command
				SendCommand( PA_CMD_INSTANT_MSG_CREATION, msgId, type.data(), msgName.data(), text.data() ) ;

				trace.printn( "Create instant message %d : <type>%s, <name>%s, <text>%s",
					msgId, type.data(), msgName.data(), text.data() ) ;
			}
		}
	}//while

	// End of instant messages configuration
	SendCommand( PA_CMD_END_INSTANT_MSG_CONFIG ) ;

	return true ;
}

//----------------------------------------------------------------------------------------------------------------------
//#BEGIN
//#METHOD bool SmsFilePaMgr::ReadMhqueue( const char *fileName )
//#COMMENT This method is used to read mhqueue file. A command is sent to server for each mhqueue line described in
// file. These commands are sent in a 'transaction' : a first command is sent to start transaction, then each mhqueue
// line commands are sent and a last command is sent to close transaction. The aim of this transaction is to know which
// mhqueue lines are no more described in mhqueue file and must be deleted.
//#END
//----------------------------------------------------------------------------------------------------------------------
bool SmsFilePaMgr::ReadMhqueue( const char *fileName )
{
	SmsTrace trace( "SmsFilePaMgr::ReadMhqueue", SMS_TRACE_PA_FTP, 0x06 ) ;

	SmsUpdatePa* updatePa = SmsUpdatePa::Get() ;

	// Open mhqueue file
    ifstream input( fileName ) ;
	if ( !input )
	{
		trace.printn( "*** Cannot open file %s ***", fileName ) ;
		return false ;
	}

	// Begin of mhqueue listing
	SendCommand( PA_CMD_BEGIN_MHQUEUE_LISTING ) ;

	RWCString zoneAddress ;
	RWCString messageField ;
	RWCString originatorName ;
	RWCString line ;
	RWCString format = updatePa->getParameters()->getQueueLineFormat() ;
	char startTime[256] ;
	int announcementId ;
	char state[256] ;
	int originatorId ;
	char msgCode[256] ;
	bool isPhysicalAddress ;
	char repetitionField[256] ;

	// Start loop to analyse each lines
	while ( line.readLine( input ) )
	{
		trace.printn( "Analysed line : %s", line.data() ) ;

		memset( startTime, 0, sizeof( startTime ) ) ;
		memset( state, 0, sizeof( state ) ) ;
		memset( msgCode, 0, sizeof( msgCode ) ) ;
		memset( repetitionField, 0, sizeof( repetitionField ) ) ;
		announcementId = 0 ;
		originatorId = 0 ;

		// Extract different fields from line
		int resSscanf = sscanf( line.data(), format.data(), startTime, &announcementId, state, &originatorId, msgCode,
			repetitionField ) ;
		if ( resSscanf < 6 )
		{
			trace.printn( "*** Syntax error : only %d parameters found in mhqueue line", resSscanf ) ;
			trace.printn( "\t<startTime> %s, <AID> %d, <state> %s, <originatorId> %d, <msgCode> %s, <repField> %s ***",
				startTime, announcementId, state, originatorId, msgCode, repetitionField ) ;
			continue ;
		}

		// Decode message code
		if ( !DecodeMsgCode( msgCode, zoneAddress, isPhysicalAddress, messageField ) )
			continue ;

		// Originator decoding
		SmsDBPaUserRef userRef ;
		if ( !updatePa->getParameters()->getUserMap()
			->getUkPaUser( SmsDBPaUser::toUkPaUserKey( originatorId ), userRef ) )
		{
			SmsDBPaOriginatorRef originatorRef ;
			if ( !updatePa->getParameters()->getOriginatorMap()
				->get( SmsDBPaOriginator::toPrimaryKey( originatorId ), originatorRef ) )
			{
				trace.printn( "*** Originator %d not found in mapping ***", originatorId ) ;
				continue ;
			}
			else
				originatorName = originatorRef->getOriginatorType() ;
		}
		else
			originatorName = userRef->getOriginatorId() ;

		// Create and send command
		SendCommand( PA_CMD_MHQUEUE_LINE_CREATION, announcementId, startTime, state, originatorName.data(),
			zoneAddress.data(), isPhysicalAddress, messageField.data(), repetitionField ) ;

		trace.printn( "Create mhqueue line %d : <startTime>%s, <state>%s, <originator>%s",
			announcementId, startTime, state, originatorName.data() ) ;
		trace.printn( "\t<zoneAddress>%s, <messageField>%s, <repetitionField>%s",
			zoneAddress.data(), messageField.data(), repetitionField ) ;
	}//while

	// End of mhqueue listing
	SendCommand( PA_CMD_END_MHQUEUE_LISTING ) ;

	return true ;
}

//----------------------------------------------------------------------------------------------------------------------
//#BEGIN
//#METHOD bool SmsFilePaMgr::ReadSchedule(
//					const char *fileName,
//					long schedDay )
//#COMMENT This method is used to read scheduler file. A command is sent to server for each scheduler line described in
// file. These commands are sent in a 'transaction' : a first command is sent to start transaction, then each scheduler
// line commands are sent and a last command is sent to close transaction. The aim of this transaction is to know which
// scheduler lines are no more described in scheduler file and must be deleted.
//#END
//----------------------------------------------------------------------------------------------------------------------
bool SmsFilePaMgr::ReadSchedule( const char *fileName, long schedDay )
{
	SmsTrace trace( "SmsFilePaMgr::ReadSchedule", SMS_TRACE_PA_FTP, 0x06 ) ;

	SmsUpdatePa* updatePa = SmsUpdatePa::Get() ;

	// Open scheduler file
    ifstream input( fileName ) ;
	if ( !input )
	{
		trace.printn( "*** Cannot open file %s ***", fileName ) ;
		return false ;
	}

	// Begin of scheduler listing
	SendCommand( PA_CMD_BEGIN_SCHEDULER_LISTING, schedDay ) ;

	RWCString zoneAddress ;
	RWCString messageField ;
	RWCString comment ;
	RWCString line ;
	RWCString format = updatePa->getParameters()->getSchedLineFormat() ;
	char startTime[256] ;
	char msgCode[256] ;
	bool isPhysicalAddress ;
	char commentStr[256] ;
	int repetitionId ;
	int lineId ;

	// Start loop to analyse each lines
	while ( line.readLine( input ) )
	{
		trace.printn( "Analysed line : %s", line.data() ) ;

		memset( startTime, 0, sizeof( startTime ) ) ;
		memset( msgCode, 0, sizeof( msgCode ) ) ;
		memset( commentStr, 0, sizeof( commentStr ) ) ;
		repetitionId = 0 ;
		lineId = 0 ;

		// Extract different fields from line
		int resSscanf = sscanf( line.data(), format.data(), startTime, msgCode, commentStr, &repetitionId, &lineId ) ;
		if ( resSscanf < 5 )
		{
			trace.printn( "*** Syntax error : only %d parameters found in scheduler line", resSscanf ) ;
			trace.printn( "\t<startTime> %s, <msgCode> %s, <comment> %s, <repId> %d, <lineId> %d ***",
				startTime, msgCode, commentStr, repetitionId, lineId ) ;
			continue ;
		}

		// Decode message code
		if ( !DecodeMsgCode( msgCode, zoneAddress, isPhysicalAddress, messageField ) )
			continue ;

		comment = commentStr ;
		SmsFileAnalyser::ExtractSpaces( comment ) ;

		// Create and send command
		SendCommand( PA_CMD_SCHEDULER_LINE_CREATION, schedDay, lineId, startTime, zoneAddress.data(), isPhysicalAddress,
			messageField.data(), comment.data() , repetitionId ) ;

		trace.printn( "Create line %d of scheduler %d : <startTime>%s, <zoneAddress>%s, <messageField>%s",
			lineId, schedDay, startTime, zoneAddress.data(), messageField.data() ) ;
		trace.printn( "\t<comment>%s, <repetitionId>%d", comment.data(), repetitionId ) ;
	}//while

	// End of scheduler listing
	SendCommand( PA_CMD_END_SCHEDULER_LISTING, schedDay ) ;

	return true ;
}

//----------------------------------------------------------------------------------------------------------------------
//#BEGIN
//#METHOD bool SmsFilePaMgr::AnalyseMsgConfigLine( RWCString line, long msgConfigSection )
//#COMMENT This method is used to analyse Pa database file line. This line will be analysed as if it belongs to opened
// section (current transaction). If it does not fit such line, it will be analysed as if it belongs to following
// sections. If it still does not succeed in any following sections, this line will be thought to have a syntax error
// and next line will be analysed (as opened section line). If it succeeds in a following section, opened section will
// be closed and new section will be opened (new transaction).
// NB : Section order is : 1. message types,
//                         2. message variables,
//                         3. predefined messages.
//#END
//----------------------------------------------------------------------------------------------------------------------
bool SmsFilePaMgr::AnalyseMsgConfigLine( RWCString line, long msgConfigSection )
{
	bool success ;

	// Switch on section this line is supposed to belong to
	switch ( msgConfigSection )
	{
		case PA_SECTION_MSG_TYPE :
			success = AnalyseMsgTypeLine( line ) ;
			break ;

		case PA_SECTION_MSG_VAR :
			success = AnalyseMsgVarLine( line ) ;
			break ;

		case PA_SECTION_PREDEF_MSG :
			success = AnalysePredefMsgLine( line, LastPredefMsgLineType ) ;
			break ;

		default :
			SmsReportEvent::Error(LOC(620),CY_PA,"Invalid message configurationfile section",msgConfigSection);
			success = false ;
			break ;
	}

	return success ;
}

//----------------------------------------------------------------------------------------------------------------------
//#BEGIN
//#METHOD bool SmsFilePaMgr::AnalyseMsgTypeLine( RWCString line )
//#COMMENT Analyse line as message type description line. If it succeeds, send message type creation command to server
// application. If it fails, try to analyse this same line as message variable description line.
//#END
//----------------------------------------------------------------------------------------------------------------------
bool SmsFilePaMgr::AnalyseMsgTypeLine( RWCString line )
{
	SmsTrace trace(	"SmsFilePaMgr::AnalyseMsgTypeLine", SMS_TRACE_PA_FTP, 0 ) ;

	// Retrieve message type description line format
	RWCString format = SmsUpdatePa::Get()->getParameters()->getMsgTypeLineFormat() ;
	int msgTypeId = 0 ;
	char msgTypeName[256] ;
	memset( msgTypeName, 0, sizeof( msgTypeName ) ) ;

	// Analyse line as message type line
	if ( sscanf( line.data(), format.data(), &msgTypeId, msgTypeName ) < 2 )
	{
		return AnalyseMsgConfigLine( line, PA_SECTION_MSG_VAR ) ;
	}

	// Checks message type id validity
	if ( ( msgTypeId < PA_MIN_MSG_TYPE_ID )||( msgTypeId > PA_MAX_MSG_TYPE_ID ) )
	{
		trace.printn( "*** Invalid message type id %d ***", msgTypeId ) ;
		return false ;
	}

	RWCString type = msgTypeName ;
	SmsFileAnalyser::ExtractSpaces( type ) ;

	// Create and send command
	SendCommand( PA_CMD_MSG_TYPE_CREATION, type.data(), msgTypeId ) ;

	trace.printn( "Create message type %s : <id>%d", type.data(), msgTypeId ) ;

	return true ;
}

//----------------------------------------------------------------------------------------------------------------------
//#BEGIN
//#METHOD bool SmsFilePaMgr::AnalyseMsgVarLine( RWCString line )
//#COMMENT Analyse line as message variable description line. If it succeeds, send message variable creation command to
// server application. If it fails, try to analyse this same line as predefined message description line.
//#END
//----------------------------------------------------------------------------------------------------------------------
bool SmsFilePaMgr::AnalyseMsgVarLine( RWCString line )
{
	SmsTrace trace(	"SmsFilePaMgr::AnalyseMsgVarLine", SMS_TRACE_PA_FTP, 0 ) ;

	// Retrieve message variable description line format
	RWCString format = SmsUpdatePa::Get()->getParameters()->getVarLineFormat() ;
	int varTypeId = 0 ;
	int varId = 0 ;
	char chineseOrEnglish[256] ;
	memset( chineseOrEnglish, 0, sizeof( chineseOrEnglish ) ) ;
	char varText[256] ;
	memset( varText, 0, sizeof( varText ) ) ;

	// Analyse line as message variable line
	if ( sscanf( line.data(), format.data(), &varTypeId, &varId, chineseOrEnglish, varText ) < 4 )
	{
		return AnalyseMsgConfigLine( line, PA_SECTION_PREDEF_MSG ) ;
	}

	if ( OpenSection == PA_SECTION_MSG_TYPE )
	// Change opened section
	{
		// End of message types configuration
		SendCommand( PA_CMD_END_MSG_TYPE_CONFIG ) ;

		// Begin of message variables configuration
		SendCommand( PA_CMD_BEGIN_MSG_VAR_CONFIG ) ;
		
		OpenSection = PA_SECTION_MSG_VAR ;
	}

	// Chinese line are ignored
	if ( !strcmp( chineseOrEnglish, "C" ) )
		trace.printn( "Chinese message variable line" ) ;
	else
	{
		RWCString text = varText ;
		SmsFileAnalyser::ExtractSpaces( text ) ;

		// Create and send command
		SendCommand( PA_CMD_MSG_VAR_CREATION, varTypeId, varId, text.data() ) ;

		trace.printn( "Create message variable %d : <type>%d, <text>%s", varId, varTypeId, text.data() ) ;
	}

	return true ;
}

//----------------------------------------------------------------------------------------------------------------------
//#BEGIN
//#METHOD bool SmsFilePaMgr::AnalysePredefMsgLine( RWCString line, long predefMsgLineType )
//#COMMENT Analyse line as predefined message description line. If it succeeds, send predefined message creation command
// to server application. If it fails, this line is thought to have a syntax error.
//#END
//----------------------------------------------------------------------------------------------------------------------
bool SmsFilePaMgr::AnalysePredefMsgLine( RWCString line, long predefMsgLineType )
{
	SmsTrace trace(	"SmsFilePaMgr::AnalysePredefMsgLine", SMS_TRACE_PA_FTP, 0 ) ;

	bool success ;

	// Try to analyse line as if it is of the same type of the last analysed one
	switch ( predefMsgLineType )
	{
		case PA_MSG_LINE_WITHOUT_VAR :
		{
			// Message without variables
			success = AnalysePredefMsgLineWithoutVar( line ) ;
		}
		break ;
		case PA_MSG_LINE_WITH_VAR :
		{
			// Message with variables
			success = AnalysePredefMsgLineWithVar( line ) ;
		}
		break ;
		case PA_MSG_LINE_WITH_VAR_END :
		{
			// Message with variables end
			success = AnalysePredefMsgLineWithVarEnd( line ) ;
		}
		break ;
		default :
		{
			SmsReportEvent::Error(LOC(966),CY_PA,"Invalid predefined message line type",predefMsgLineType);
			success = false ;
		}
	}

	return success ;
}

//----------------------------------------------------------------------------------------------------------------------
//#BEGIN
//#METHOD bool SmsFilePaMgr::AnalysePredefMsgLineWithoutVar( RWCString line )
//#COMMENT Analyse line as description of a predefined message without variables. If it succeeds, send predefined
// message creation command to server application. If it fails, try to analyse it as description of a predefined message
// with variables unless it has already been done.
//#END
//----------------------------------------------------------------------------------------------------------------------
bool SmsFilePaMgr::AnalysePredefMsgLineWithoutVar( RWCString line )
{
	SmsTrace trace(	"SmsFilePaMgr::AnalysePredefMsgLineWithoutVar", SMS_TRACE_PA_FTP, 0 ) ;

	SmsUpdatePa* updatePa = SmsUpdatePa::Get() ;

	// Retrieve predefined message description line format
	RWCString format = updatePa->getParameters()->getMsgWithoutVarLineFormat() ;
	char msgType[256] ;
	memset( msgType, 0, sizeof( msgType ) ) ;
	int msgId = 0 ;
	char chineseOrEnglish[256] ;
	memset( chineseOrEnglish, 0, sizeof( chineseOrEnglish ) ) ;
	char msgText[256] ;
	memset( msgText, 0, sizeof( msgText ) ) ;

	// Analyse line as predefined message line
	if ( sscanf( line.data(), format.data(), msgType, &msgId, chineseOrEnglish, msgText ) < 4 )
	{
		// Check result
		if ( LastPredefMsgLineType != PA_MSG_LINE_WITH_VAR )
			return AnalysePredefMsgLine( line, PA_MSG_LINE_WITH_VAR ) ;
		else
			return false ;
	}

	// Save type of the analysed predefined message line
	LastPredefMsgLineType = PA_MSG_LINE_WITHOUT_VAR ;

	// Check if opened section must be changed
	if ( ( OpenSection == PA_SECTION_MSG_TYPE )||( OpenSection == PA_SECTION_MSG_VAR ) )
	{
		// End of open section
		if ( OpenSection == PA_SECTION_MSG_TYPE )
			// End of message types configuration
			SendCommand( PA_CMD_END_MSG_TYPE_CONFIG ) ;
		else
			// End of message variables configuration
			SendCommand( PA_CMD_END_MSG_VAR_CONFIG ) ;

		// Begin of predefined messages configuration
		SendCommand( PA_CMD_BEGIN_PREDEF_MSG_CONFIG ) ;

		OpenSection = PA_SECTION_PREDEF_MSG ;
	}

	// Chinese line are ignored
	if ( !strcmp( chineseOrEnglish, "C" ) )
		trace.printn( "Chinese message config line" ) ;
	else
	{
		RWCString text = msgText ;
		RWCString type = msgType ;
		SmsFileAnalyser::ExtractSpaces( type ) ;

		// Extract message title from its text
		RWCString msgName = ReadTitle( text ) ;

		// Create and send command
		SendCommand( PA_CMD_MSG_PART_CREATION, msgId, type.data(), msgName.data(), text.data(), 1 ) ;

		trace.printn( "Create predef message %d : <type>%s, <name>%s, <text>%s",
			msgId, type.data(), msgName.data(), text.data() ) ;
	}

	return true ;
}

//----------------------------------------------------------------------------------------------------------------------
//#BEGIN
//#METHOD bool SmsFilePaMgr::AnalysePredefMsgLineWithVar( RWCString line )
//#COMMENT Analyse line as description of a predefined message with variables. If it succeeds, send predefined message
// part creation command to server application. If it fails, try to analyse it as description of an end of predefined
// message with variables unless it has already been done.
//#END
//----------------------------------------------------------------------------------------------------------------------
bool SmsFilePaMgr::AnalysePredefMsgLineWithVar( RWCString line )
{
	SmsTrace trace(	"SmsFilePaMgr::AnalysePredefMsgLineWithVar", SMS_TRACE_PA_FTP, 0 ) ;

	SmsUpdatePa* updatePa = SmsUpdatePa::Get() ;

	// Retrieve predefined message with variables description line format
	RWCString format = updatePa->getParameters()->getMsgWithVarLineFormat() ;
	char msgType[256] ;
	memset( msgType, 0, sizeof( msgType ) ) ;
	int msgId = 0 ;
	int msgPart = 0 ;
	char chineseOrEnglish[256] ;
	memset( chineseOrEnglish, 0, sizeof( chineseOrEnglish ) ) ;
	int varType = 0 ;
	char msgText[256] ;
	memset( msgText, 0, sizeof( msgText ) ) ;

	// Analyse line as predefined message line
	if ( sscanf( line.data(), format.data(), msgType, &msgId, &msgPart, chineseOrEnglish, &varType, msgText ) < 6 )
	{
		// Check result
		if ( LastPredefMsgLineType != PA_MSG_LINE_WITH_VAR_END )
			return AnalysePredefMsgLine( line, PA_MSG_LINE_WITH_VAR_END ) ;
		else
			return false ;
	}

	// Save type of the analysed predefined message line
	LastPredefMsgLineType = PA_MSG_LINE_WITH_VAR ;

	// Check if opened section must be changed
	if ( ( OpenSection == PA_SECTION_MSG_TYPE )||( OpenSection == PA_SECTION_MSG_VAR ) )
	{
		// End of open section
		if ( OpenSection == PA_SECTION_MSG_TYPE )
			// End of message types configuration
			SendCommand( PA_CMD_END_MSG_TYPE_CONFIG ) ;
		else
			// End of message variables configuration
			SendCommand( PA_CMD_END_MSG_VAR_CONFIG ) ;

		// Begin of predefined messages configuration
		SendCommand( PA_CMD_BEGIN_PREDEF_MSG_CONFIG ) ;

		OpenSection = PA_SECTION_PREDEF_MSG ;
	}

	// Chinese line are ignored
	if ( !strcmp( chineseOrEnglish, "C" ) )
		trace.printn( "Chinese message config line" ) ;
	else
	{
		RWCString text = msgText ;
		RWCString type = msgType ;
		RWCString msgName = "" ;
		SmsFileAnalyser::ExtractSpaces( type ) ;

		// Extract message name from first part
		if ( msgPart == 1 )
			// Extract message title from its first part text
			msgName = ReadTitle( text ) ;
		else
			SmsFileAnalyser::ExtractSpaces( text ) ;

		// Create and send command
		SendCommand( PA_CMD_MSG_PART_CREATION, msgId, type.data(), msgName.data(), text.data(), msgPart, varType ) ;

		trace.printn( "Create part %d of predef message %d : <type>%s, <name>%s, <text>%s, <varType>%d",
			msgPart, msgId, type.data(), msgName.data(), text.data(), varType ) ;
	}

	return true ;
}

//----------------------------------------------------------------------------------------------------------------------
//#BEGIN
//#METHOD bool SmsFilePaMgr::AnalysePredefMsgLineWithVarEnd( RWCString line )
//#COMMENT Analyse line as description of the end of a predefined message with variables. If it succeeds, send last
// predefined message part creation command to server application. If it fails, try to analyse it as description of a
// predefined message without variables unless it has already been done.
//#END
//----------------------------------------------------------------------------------------------------------------------
bool SmsFilePaMgr::AnalysePredefMsgLineWithVarEnd( RWCString line )
{
	SmsTrace trace(	"SmsFilePaMgr::AnalysePredefMsgLineWithVarEnd", SMS_TRACE_PA_FTP, 0 ) ;

	SmsUpdatePa* updatePa = SmsUpdatePa::Get() ;

	// Retrieve end of predefined message with variables description line format
	RWCString format = updatePa->getParameters()->getMsgWithVarEndLineFormat() ;
	char msgType[256] ;
	memset( msgType, 0, sizeof( msgType ) ) ;
	int msgId = 0 ;
	int msgPart = 0 ;
	char chineseOrEnglish[256] ;
	memset( chineseOrEnglish, 0, sizeof( chineseOrEnglish ) ) ;
	char msgText[256] ;
	memset( msgText, 0, sizeof( msgText ) ) ;

	// Analyse line as predefined message line
	if ( sscanf( line.data(), format.data(), msgType, &msgId, &msgPart, chineseOrEnglish, msgText ) < 5 )
	{
		// Check result
		if ( LastPredefMsgLineType != PA_MSG_LINE_WITHOUT_VAR )
			return AnalysePredefMsgLine( line, PA_MSG_LINE_WITHOUT_VAR ) ;
		else
			return false ;
	}

	// Save type of the analysed predefined message line
	LastPredefMsgLineType = PA_MSG_LINE_WITH_VAR_END ;

	// Check if opened section must be changed
	if ( ( OpenSection == PA_SECTION_MSG_TYPE )||( OpenSection == PA_SECTION_MSG_VAR ) )
	{
		// End of open section
		if ( OpenSection == PA_SECTION_MSG_TYPE )
			// End of message types configuration
			SendCommand( PA_CMD_END_MSG_TYPE_CONFIG ) ;
		else
			// End of message variables configuration
			SendCommand( PA_CMD_END_MSG_VAR_CONFIG ) ;

		// Begin of predefined messages configuration
		SendCommand( PA_CMD_BEGIN_PREDEF_MSG_CONFIG ) ;

		OpenSection = PA_SECTION_PREDEF_MSG ;
	}

	// Chinese line are ignored
	if ( !strcmp( chineseOrEnglish, "C" ) )
		trace.printn( "Chinese message config line" ) ;
	else
	{
		RWCString text = msgText ;
		RWCString type = msgType ;

		SmsFileAnalyser::ExtractSpaces( text ) ;
		SmsFileAnalyser::ExtractSpaces( type ) ;

		// Create and send command
		SendCommand( PA_CMD_MSG_PART_CREATION, msgId, type.data(), "", text.data(), msgPart ) ;

		trace.printn( "Create last part (%d) of predef message %d : <type>%s, <text>%s",
			msgPart, msgId, type.data(), text.data() ) ;
	}

	return true ;
}

//----------------------------------------------------------------------------------------------------------------------
//#BEGIN
//#METHOD void SmsFilePaMgr::SendCommand(
//					long cmdId,
//					IlsMvValue param1,
//					IlsMvValue param2,
//					IlsMvValue param3,
//					IlsMvValue param4,
//					IlsMvValue param5,
//					IlsMvValue param6,
//					IlsMvValue param7,
//					IlsMvValue param8 )
//#COMMENT This method is used to send a command to server application. It calls a callback into update Ilog server
// main loop.
//#END
//----------------------------------------------------------------------------------------------------------------------
void SmsFilePaMgr::SendCommand( long cmdId, IlsMvValue param1, IlsMvValue param2, IlsMvValue param3, IlsMvValue param4,
							   IlsMvValue param5, IlsMvValue param6, IlsMvValue param7, IlsMvValue param8 )
{
    /// MB 17/12/02 : fuite mémoire UpdatePA
    /*
	IlsMvValue args[9] ;
	args[0] = cmdId ;
	args[1] = param1 ;
	args[2] = param2 ;
	args[3] = param3 ;
	args[4] = param4 ;
	args[5] = param5 ;
	args[6] = param6 ;
	args[7] = param7 ;
	args[8] = param8 ;

	// Call function into main loop
	SmsUpdatePa::Get()->getFtpComponent()->execAsyncGlobalCallback( "sendCmdFilePa", args, 9 ) ;
    */

	// Create new command to be sent
	SmsCommandPaP cmd = new SmsCommandPa ;

	// Set command identifier
	cmd->id = cmdId ;

	// Set parameters
	cmd->param1 = param1 ;
	cmd->param2 = param2 ;
	cmd->param3 = param3 ;
	cmd->param4 = param4 ;
	cmd->param5 = param5 ;
	cmd->param6 = param6 ;
	cmd->param7 = param7 ;
	cmd->param8 = param8 ;

	// Send command
	SmsCommandManager::SendCommand(*cmd) ;
}

//----------------------------------------------------------------------------------------------------------------------
//#BEGIN
//#METHOD bool SmsFilePaMgr::BuildMask(
//					const char *physicalAddressLine,
//					RWCString &mask,
//					int &physicalAddress )
//#COMMENT Build zone mask from list of physical addresses. {\i physicalAddress} is null for zone groups.
//#END
//----------------------------------------------------------------------------------------------------------------------
bool SmsFilePaMgr::BuildMask( const char *physicalAddressLine, RWCString &mask, int &physicalAddress )
{
	SmsTrace trace( "SmsFilePaMgr::BuildMask", SMS_TRACE_PA_FTP, 0 ) ;

	unsigned long maskMSB = 0 ;
	unsigned long maskLSB = 0 ;
	int singleZone = 0 ;
	RWCString physicalAddressList = physicalAddressLine ;

	// Read physical address list to build mask
	while ( !physicalAddressList.isNull() )
	{
		singleZone++ ;
		RWCString physicalAddressStr = SmsFileAnalyser::ReadToDelim( physicalAddressList, ',' ) ;
		SmsFileAnalyser::ExtractToDelim( physicalAddressList, ',' ) ;
		physicalAddress = atoi( physicalAddressStr.data() ) ;

		if ( ( physicalAddress > 64 ) || ( physicalAddress < 1 ) )
		{
			trace.printn( "*** Invalid physical address %d ***", physicalAddress ) ;
		}
		else
		{
			if ( physicalAddress > 32 )
				maskMSB |= ( 1 << ( physicalAddress - 33 ) ) ;
			else
				maskLSB |= ( 1 << ( physicalAddress - 1 ) ) ;
		}
	}

	// Check if there is at least one valid physical address in list
	if ( ( maskLSB == 0 )&&( maskMSB == 0 ) )
		return false ;

	// Physical address for group is set to zero
	if ( singleZone != 1 )
		physicalAddress = 0 ;

	// Format mask
	char maskStr[256] ;
	sprintf( maskStr, "%08X%08X", maskMSB, maskLSB ) ;
	mask = maskStr ;

	return true ;
}

//----------------------------------------------------------------------------------------------------------------------
//#BEGIN
//#METHOD RWCString SmsFilePaMgr::ReadTitle( RWCString &text )
//#COMMENT Read message title from its first part text. If message text contains ':', message title is the part before
// this character. This part is extracted from message text. Otherwise, message title will be the first words of message
// text followed by '...'.
//#END
//----------------------------------------------------------------------------------------------------------------------
RWCString SmsFilePaMgr::ReadTitle( RWCString &text )
{
	RWCString msgName ;
	if ( text.contains( ":" ) )
	// Extract message name
	{
		msgName = SmsFileAnalyser::ReadToDelim( text.data(), ':' ) ;
		SmsFileAnalyser::ExtractToDelim( text, ':' ) ;
	}
	else
	// Extract message name from message text (first words)
	{
		RWCString msgNameField = SmsFileAnalyser::ReadBegin( text.data(),
			SmsUpdatePa::Get()->getParameters()->getDefaultMsgNameLength() ) ;
		if ( msgNameField != text )
		{
			SmsFileAnalyser::ExtractEnd( msgNameField, 2 ) ;
			msgName = SmsFileAnalyser::ReadToLastDelim( msgNameField.data(), ' ' ) + "..." ;
		}
		else
			msgName = msgNameField ;
	}
	SmsFileAnalyser::ExtractSpaces( msgName ) ;
	SmsFileAnalyser::ExtractSpaces( text ) ;

	return msgName ;
}

//----------------------------------------------------------------------------------------------------------------------
//#BEGIN
//#METHOD bool SmsFilePaMgr::DecodeMsgCode(
//					const char *msgCode,
//					RWCString &zoneAddress,
//					bool &isPhysicalAddress,
//					RWCString &messageField )
//#COMMENT Extract zone address and message field from message code. Zone address could be a mask (physical address) or
// zone name.
//#END
//----------------------------------------------------------------------------------------------------------------------
bool SmsFilePaMgr::DecodeMsgCode( const char *msgCode, RWCString &zoneAddress, bool &isPhysicalAddress,
								 RWCString &messageField )
{
	SmsTrace trace( "SmsFilePaMgr::DecodeMsgCode", SMS_TRACE_PA_FTP, 0x06 ) ;

	RWCString msgCodeFormat = SmsUpdatePa::Get()->getParameters()->getMsgCodeFormat() ;
	char zoneAddressStr[256] ;
	char messageFieldStr[256] ;
	memset( zoneAddressStr, 0, sizeof( zoneAddressStr ) ) ;
	memset( messageFieldStr, 0, sizeof( messageFieldStr ) ) ;

	// Extract different fields from message code
	int resSscanf = sscanf( msgCode, msgCodeFormat.data(), zoneAddressStr, messageFieldStr ) ;
	if ( resSscanf < 2 )
	{
		trace.printn( "*** Syntax error : only %d parameters found in message code string", resSscanf ) ;
		trace.printn( "\t<zoneAddress> %s, <messageField> %s ***", zoneAddressStr, messageFieldStr ) ;
		return false ;
	}

	messageField = messageFieldStr ;

	// Address decoding
	zoneAddress = zoneAddressStr ;
	if ( SmsFileAnalyser::StartWith( zoneAddress, "X" ) )
	// Physical address (mask)
	{
		isPhysicalAddress = true ;
		SmsFileAnalyser::ExtractBegin( zoneAddress, 1 ) ;
	}
	else
	// Logical address
	{
		isPhysicalAddress = false ;
		int logicalAddress ;

		// Check if this logical address is special code (from handsets)
		if ( SmsFileAnalyser::EndWith( zoneAddress, "*" ) )
		{
			// Build real logical address
			SmsFileAnalyser::ExtractEnd( zoneAddress, 1 ) ;
			SmsFileAnalyser::ExtractBegin( zoneAddress, 1 ) ;
			logicalAddress = 100 + atoi( zoneAddress.data() ) ;
		}
		else
			logicalAddress = atoi( zoneAddress.data() ) ;

		// Find zone name
		SmsDBPaZoneRef zoneRef ;
		if ( !SmsUpdatePa::Get()->getParameters()->getZoneMap()
			->getUkKeyPaZone( SmsDBPaZone::toUkKeyPaZoneKey( logicalAddress ), zoneRef ) )
		{
			trace.printn( "*** Zone %d not found in mapping ***", logicalAddress ) ;
			return false ;
		}
		zoneAddress = zoneRef->getEquipmentId() ;
	}

	return true ;
}

//----------------------------------------------------------------------------------------------------------------------
