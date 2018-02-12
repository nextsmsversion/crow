#include "crow.h"

// Standards by sam 20180212
#include <iostream>

int main()
{
    crow::SimpleApp app;

    CROW_ROUTE(app, "/")
    ([]() {
        std::filebuf fb;
        if (fb.open ("/home/ubuntu/workspace/examples/msgConfig.txt",std::ios::in))
        {
            return "SMS 2.0 Container app! Read File Success";
        }
        return "SMS 2.0 Container app! ";
    });
    
    app.port(8080).run();
}

std::string ReadMsgConfig( const char *fileName ){
    return "";
}