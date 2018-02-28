#include "crow.h"

// Standards by sam 20180212
#include <fstream>
#include <iostream>
#include <string>
#include <stdio.h>

using namespace std;

#define PA_SECTION_UNKNOWN      0

#define PA_SECTION_MSG_TYPE     1
#define PA_SECTION_MSG_VAR      2
#define PA_SECTION_PREDEF_MSG   3

#define PA_MIN_MSG_TYPE_ID      1
#define PA_MAX_MSG_TYPE_ID      10

crow::json::wvalue jsonMsgType;

//----------------------------------------------------------------------------------------------------------------------
//#BEGIN
//#METHOD bool AnalyseMsgTypeLine( RWCString line )
//#COMMENT Analyse line as message type description line. If it succeeds, send message type creation command to server
// application. If it fails, try to analyse this same line as message variable description line.
//#END
//----------------------------------------------------------------------------------------------------------------------
bool AnalyseMsgTypeLine( string line )
{
	//tmp comment out  CROW_LOG_INFO <<	"SmsFilePaMgr::AnalyseMsgTypeLine"  ;

	// Retrieve message type description line format
	string format ="%2d%*1[	]%200c"; // SmsUpdatePa::Get()->getParameters()->getMsgTypeLineFormat() ;
	int msgTypeId = 0 ;
	char msgTypeName[256] ;
	memset( msgTypeName, 0, sizeof( msgTypeName ) ) ;

	// Analyse line as message type line
	if ( sscanf( line.c_str(), format.c_str(), &msgTypeId, msgTypeName ) < 2 )
	{
		//TODO  return AnalyseMsgConfigLine( line, PA_SECTION_MSG_VAR ) ;
	}

	// Checks message type id validity
	if ( ( msgTypeId < PA_MIN_MSG_TYPE_ID )||( msgTypeId > PA_MAX_MSG_TYPE_ID ) )
	{
		//tmp comment out CROW_LOG_INFO << "*** Invalid message type id" <<   msgTypeId  << " ***";
		return false ;
	}


	string type = msgTypeName ;
    type.erase(type.find_last_not_of(" \n\r\t")+1); //replace SmsFileAnalyser::ExtractSpaces( type ) ;
    
	// replace: Create and send command (to be replaced by RESTful API)
	// replace: SendCommand( PA_CMD_MSG_TYPE_CREATION, type.data(), msgTypeId ) ;
	/** originally by sam 20180228 **/
    jsonMsgType["msgType"][to_string(msgTypeId)] = type;	
    //jsonMsgType[to_string(msgTypeId)] = type;
    
	CROW_LOG_INFO << "Create message type <" <<  type  << ">"; // TODO : <id>%d" msgTypeId 

	return true ;
}

bool AnalyseMsgConfigLine( std::string line, long msgConfigSection )
{
	bool success = false;

	// Switch on section this line is supposed to belong to
	switch ( msgConfigSection )
	{
		case PA_SECTION_MSG_TYPE :
			success = AnalyseMsgTypeLine( line ) ;
			break ;

		case PA_SECTION_MSG_VAR :
			//success = AnalyseMsgVarLine( line ) ;
			break ;

		case PA_SECTION_PREDEF_MSG :
			//success = AnalysePredefMsgLine( line, LastPredefMsgLineType ) ;
			break ;

		default :
			success = false ;
			break ;
	}
	return success ;
}


string ReadMsgConfig(const string& msg)
{
    
    std::string line;
    std::ifstream pafile (msg, std::ifstream::in);
    
    long OpenSection = PA_SECTION_MSG_TYPE; // ?? PA_SECTION_UNKNOWN ;
    
    while(getline(pafile, line)) {
        
        // Analyse line
		if ( !AnalyseMsgConfigLine( line, OpenSection ) )
		{
			//CROW_LOG_INFO <<  "*** Syntax error ***" ;
		}else{
		    CROW_LOG_INFO << "Analysed line : " << line;
		}
    }
    //TODO add more here?? jsonMsgType["last"] = "DONE";
    return crow::json::dump(jsonMsgType);
}

int main()
{
    crow::SimpleApp app;

    CROW_ROUTE(app, "/")
    ([]() {
        
        return ReadMsgConfig("/home/ubuntu/workspace/examples/msgConfig.txt");
    });
    
    //TODO by sam retrieve from PA server text file
    jsonMsgType["messages"][to_string(1)] = "Last Train to";
    jsonMsgType["messages"][to_string(2)] = "Last Train has ended";
    jsonMsgType["messages"][to_string(3)] = "Last train at platform will depart at";
    
    CROW_ROUTE(app, "/msgs")
    ([](const crow::request& /*req*/, crow::response& res){
    		ReadMsgConfig("/home/ubuntu/workspace/examples/msgConfig.txt");
            res.write(crow::json::dump(jsonMsgType));
            res.end();
    });
    
    app.port(8080).run();
}

