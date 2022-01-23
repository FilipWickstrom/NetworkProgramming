#include "sspLib.h"

bool RecvMSG(const int& sock, std::string& buffer)
{
    bool gotMSG = true;
    char msg[MAXMSGSIZE];

    int bytes = recv(sock, msg, sizeof(msg)-1, 0);
    //We got a message
    if (bytes > 0)
    {
        msg[bytes] = '\0';
        buffer.append(msg);

        //If we recieve a message with quit we shall not read anymore
        if (GetKeyword(msg) == Keyword::QUIT)
        {
            gotMSG = false;
        }
    }
    //Empty input means that the other side has closed down
    else if (bytes == 0)
    {
        gotMSG = false;
    }
    return gotMSG;
}

std::string GetMSG(std::string& buffer, bool removeMsg)
{
    std::string finalMSG = "";

    //Search for an <END>
    size_t endStartIndex = buffer.find(END);
    if (endStartIndex != std::numeric_limits<size_t>::max())
    {
        //Get the first part
        finalMSG = buffer.substr(0, endStartIndex);
        
        //Delete this part from the buffer
        if (removeMsg)
            buffer.erase(0, endStartIndex + END.length());
    }
    return finalMSG;
}

void EraseLastMSG(std::string& buffer)
{
    size_t endStartIndex = buffer.find(END);
    if (endStartIndex != std::numeric_limits<size_t>::max())
    {
        //Delete this part from the buffer
        buffer.erase(0, endStartIndex + END.length());
    }
}

void ClearMSGQueue(std::string& buffer)
{
    buffer = "";
}

void SendMSG(const int& sock, std::string msg)
{
    //Can't send empty messages
    if (msg != "")
    {
        msg.append(END);
        if (send(sock, &msg[0], msg.length(), 0) < 0)
            std::cout << "Failed to send message: '" << msg << "' to socket: " << sock << "..." << std::endl;
    }
}

Keyword GetKeyword(const std::string& fullText)
{
    Keyword finalKey = Keyword::NONE;

    //Search for the keyword and return it
    //key: first = enum class Keyword, second = std::string
    for (auto const& key : KEYS)
    {
        if (fullText.find(key.second) != std::numeric_limits<size_t>::max())
        {
            finalKey = key.first;
        }
    }
    return finalKey;
}

std::string GetKeyMSG(const std::string& fullText, const Keyword& key)
{
    std::string msg = "";
    Keyword correctKey = GetKeyword(fullText);

    if (correctKey == key)
    {
        int keyStart = 0;
        int keyEnd = keyStart + KEYS.at(key).length();
        msg = fullText.substr(keyEnd);
    }
    else
    {
        std::cout << "Keys did not match..." << std::endl;
    }
    return msg;
}

bool GetIpAndPort(struct sockaddr_storage addr, std::string& host, std::string& port)
{
    char hoststr[NI_MAXHOST];
    char portstr[NI_MAXSERV];
    socklen_t addr_size = sizeof(addr);

    int rc = getnameinfo((struct sockaddr *)&addr, 
                        addr_size, hoststr, sizeof(hoststr), 
                        portstr, sizeof(portstr), 
                        NI_NUMERICHOST | NI_NUMERICSERV);

    bool status = false;
    if (rc == 0)
    {
        host = hoststr;
        port = portstr;
        status = true;
    }
    return status;
}

std::string RegexNameCheck(const std::string& username)
{
    std::string errorMSG = "";

    //Check username length
    if (username.length() > MAXNAMELENGTH)
    {
        errorMSG = "ERROR: Username to long... Max length: " + std::to_string(MAXNAMELENGTH);  
    }
    else
    {
        //Holds the acceptable characters that can be used
        regex_t regularexpression;

        //If regcomp returns 0 it was all good. Anything else means error...
        //A-Z, a-z, 0-9 and _ is okay characters
        if(regcomp(&regularexpression, "^[A-Za-z0-9_]+$", REG_EXTENDED))
        {
            errorMSG = "ERROR: Regex failed to compile...";
        }
        else
        {
            int error = regexec(&regularexpression, username.c_str(), size_t(0), NULL, 0);
            regfree(&regularexpression);

            //When error == 0 there is no errors
            if (error != 0)
            {
                errorMSG = "ERROR: Username '" + username + "' was not accepted...";
            }
        }
    }
    return errorMSG;
}
