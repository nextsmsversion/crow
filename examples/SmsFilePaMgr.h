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
bool AnalyseMsgConfigLine( std::string line, long msgConfigSection )
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
