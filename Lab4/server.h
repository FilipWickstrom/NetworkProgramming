#include <unordered_map>
#include <chrono>       //Timer
#include <pthread.h>    //Using threads for the games
#include <fstream>      //Read & write to highscore
#include <algorithm>    //Sort highscore

//Helpfunctions
#include "sspLib.h"

const std::string     PROTOCOL            = "RPS_TCP_2021";
//How many connections that can queue up at the same time
const unsigned int    BACKLOG             = 5;
//Maximum number of players in a game
const unsigned int    MAX_GAME_USERS      = 2;
const unsigned int    MAX_GAME_SPECTATORS = 4;
const double          MAX_RESPONSTIME     = 2; //In seconds
const unsigned int    MAX_HIGHSCORE_USERS = 5;
volatile sig_atomic_t sigInt              = 0;

enum class UserType
{
    NONE,
    PLAYER,
    SPECTATOR
};

struct User
{
    std::string name        = "";
    int socket              = -1;
    PlayerState state       = PlayerState::MENU_SEND;
    UserType type           = UserType::NONE;
    bool readyToStart       = false;
    std::string msgBuffer   = "";
};

enum class GameState
{
    READY,
    GOING,
    ENDED
};

struct Game
{
    User* users[MAX_GAME_USERS]           = {nullptr};
    User* spectators[MAX_GAME_SPECTATORS] = {nullptr};
    GameState playState                   = GameState::READY;
    RPSChoice choice[MAX_GAME_USERS]      = {RPSChoice::NONE};
    pthread_t thread;
    void* server                          = nullptr;
};

enum class Winner
{
    NONE,
    PLAYER1,
    PLAYER2
};

struct HighscoreData
{
    std::string username = "<NONE>";
    double responstime   = MAX_RESPONSTIME;

    bool operator < (const HighscoreData& data) const
    {
        return (this->responstime < data.responstime);
    }
};

class Server
{
private:
    bool m_isRunning;
    int m_masterSocket;
    bool m_debugMode;

    //All the users with their information
    std::unordered_map<std::string, User*> m_users;

    //Queue to erase an user from an unordered_map
    std::vector<std::string> m_leaveQueue;

    //A list of games
    std::vector<Game*> m_games;

//Helpfunctions
private:
    //Check incoming users
    bool CheckIncoming();

    //Sets for select
    int PrepareSet(fd_set& fds);
    void CloseSet(fd_set& fds);

    //Get info
    int GetGameNr(const std::string& name);
    int GetNrOfUsers(Game* game);
    int GetNrOfSpectators(Game* game);

    //Play the game thread
    static void* PlayGameThread(void* arg);
    void PlayGame(Game* game);
    Winner GameLogic(Game* game, std::string msg[2]);
    void RemoveUserFromGame(const std::string& name);
    void CloseGame(const std::string& leavingPlayer); 

    //Menu
    void SendMenu(const std::string& name);
    void RecvMenu(const std::string& name, const std::string& msg);
    void ReturnUserToMenu(User* user);

    //Gamequeue
    void SendGameQueue(User* user);
    void RecvGameQueue(User* user, const std::string& msg);
    void RecvGameQueueReady(const std::string& name, const std::string& msg);

    //Highscore
    std::vector<HighscoreData> ReadHighFile(const std::string& filename);
    void SaveToHighscore(const std::vector<HighscoreData> inputData);
    void SendHighscore(User* user);
    
    //Send message to both spectator and players
    void SendGameMSG(const Game* game, std::string msg);
    void SendGamePlayerMSG(const Game* game, std::string msg);
    void SendSpectatorMSG(const Game* game, std::string msg);

    //Remove users from unordered_map
    void ClearLeaveQueue();

    //Help when using sigint (Ctrl+C)
    static void SignumCall(int signum);

public:
    Server();
    ~Server();

    //Setup the socket and address to listen on.
    //Works for both IPv4 and IPv6
    bool Setup(std::string address, std::string port);
    
    //Go through all the clients and see what they are going to do
    void Run();
};
