#include "server.h"

Server::Server()
{
    m_isRunning = true;
    m_debugMode = false;
}

Server::~Server()
{
    for (auto& user : m_users)
        m_leaveQueue.push_back(user.second->name);

    for (int i = 0; i < m_leaveQueue.size(); i++)
    {
        User* user = m_users[m_leaveQueue[i]];
        if (GetGameNr(user->name) != -1)
        {
            CloseGame(user->name);
        }
        close(user->socket);
        delete m_users[m_leaveQueue[i]];
    }
    m_leaveQueue.clear();
    m_users.clear();
    m_games.clear();
    close(m_masterSocket);
}

bool Server::CheckIncoming()
{
    struct sockaddr_storage clientAddr;
    socklen_t client_size = sizeof(clientAddr);
    User* newUser = new User();
    std::string clientMSG = "";

    newUser->socket = accept(m_masterSocket, (struct sockaddr *)&clientAddr, &client_size);
    std::string host = "";
    std::string port = "";

    if (GetIpAndPort(clientAddr, host, port))
        std::cout << "\n[+] Client connected from address: " << host << " port: " << port << std::endl;
    else
        std::cout << "[-] Failed to get address and port..." << std::endl;

    //Recv protocol from client
    if (!RecvMSG(newUser->socket, newUser->msgBuffer))
    {
        std::cout << "[-] Client left..." << std::endl;
        close(newUser->socket);
        delete newUser;
        return false;
    }
    
    clientMSG = GetMSG(newUser->msgBuffer, true);
    if (clientMSG == "")
    {
        close(newUser->socket);
        delete newUser;
        return false;
    }
    else if (clientMSG == PROTOCOL)
    {
        std::cout << "[+] Protocol '" << clientMSG << "' supported" << std::endl;
        SendMSG(newUser->socket, KEYS.at(Keyword::OK));
    }
    else
    {
        std::cout << "[-] Protocol '" << clientMSG << "' not supported...\n" << std::endl;
        SendMSG(newUser->socket, KEYS.at(Keyword::ERROR) + "Protocol not supported...");
        close(newUser->socket);
        delete newUser;
        return false;
    }
    
    //Check the username
    if (!RecvMSG(newUser->socket, newUser->msgBuffer))
    {
        std::cout << "[-] Client left..." << std::endl;
        close(newUser->socket);
        delete newUser;
        return false;
    }

    clientMSG = GetMSG(newUser->msgBuffer, true);
    if (clientMSG == "")
    {
        close(newUser->socket);
        delete newUser;
        return false;
    }
    newUser->name = clientMSG;

    //Check if the username follow the rules
    std::string errorMSG = RegexNameCheck(newUser->name);
    if (errorMSG == "")
    {
        //Search trough all the usernames on the server
        //If it already exist, we should send back an error msg
        if (m_users.find(newUser->name) != m_users.end())
        {
            errorMSG = "ERROR: Username was already taken...";
            std::cout << "[-] " << errorMSG << '\n' << std::endl;
            SendMSG(newUser->socket, KEYS.at(Keyword::ERROR) + errorMSG);
            close(newUser->socket);
            delete newUser;
            return false;
        }
        else
        {
            SendMSG(newUser->socket, KEYS.at(Keyword::OK));

            //Adding the user to the server
            m_users[newUser->name] = newUser;
            std::cout << "[+] " << newUser->name << " joined the server on socket: " << newUser->socket << "\n" << std::endl;
        }
    }
    else
    {
        std::cout << "[-] " << errorMSG << '\n' << std::endl;
        SendMSG(newUser->socket, KEYS.at(Keyword::ERROR) + errorMSG);
        close(newUser->socket);
        delete newUser;
        return false;
    }

    return true;
}

int Server::PrepareSet(fd_set& fds)
{
    FD_ZERO(&fds);
    FD_SET(m_masterSocket, &fds);
    int maxFd = m_masterSocket;
    for (auto& user: m_users)
    {
        int userSock = user.second->socket;
        FD_SET(userSock, &fds);
        if (userSock > maxFd)
            maxFd = userSock;
    }
    return maxFd;
}

void Server::CloseSet(fd_set& fds)
{
    for (auto& user : m_users)
    {
        FD_CLR(user.second->socket, &fds);
    }
    FD_CLR(m_masterSocket, &fds);
    FD_ZERO(&fds);
}

int Server::GetGameNr(const std::string& name)
{
    int index = -1;
    bool foundIndex = false;
    for (int g = 0; g < m_games.size() && !foundIndex; g++)
    {
        const Game* game = m_games[g];

        //Go through all the active users
        for (int u = 0; u < MAX_GAME_USERS; u++)
        {
            if (game->users[u] != nullptr)
            {
                if (game->users[u]->name == name)
                {
                    index = g;
                    foundIndex = true;
                }
            }
        }

        //Go through all the active spectators
        for (int s = 0; s < MAX_GAME_SPECTATORS; s++)
        {
            if (game->spectators[s] != nullptr)
            {
                if (game->spectators[s]->name == name)
                {
                    index = g;
                    foundIndex = true;
                }
            }
        }
    }
    return index;
}

int Server::GetNrOfUsers(Game* game)
{
    int nrOfUsers = 0;
    for (int u = 0; u < MAX_GAME_USERS; u++)
    {
        if (game->users[u] != nullptr)
            nrOfUsers++;
    }
    return nrOfUsers;
}

int Server::GetNrOfSpectators(Game* game)
{
    int nrOfSpec = 0;
    for (int s = 0; s < MAX_GAME_SPECTATORS; s++)
    {
        if (game->spectators[s] != nullptr)
            nrOfSpec++;
    }
    return nrOfSpec;
}

void* Server::PlayGameThread(void* arg)
{
    //Start the game loop by calling the class function 
    Game* game = static_cast<Game*>(arg);
    static_cast<Server*>(game->server)->PlayGame(game);
    return nullptr;
}

void Server::PlayGame(Game* game)
{
    game->playState = GameState::GOING;
    User* user1 = game->users[0];
    User* user2 = game->users[1];
    auto roundStart = std::chrono::high_resolution_clock::now();
    int round = 1;

    bool phase[4] = {false};
    int score[MAX_GAME_USERS] = {0};
    std::string input[MAX_GAME_USERS] = {""};

    //For reactiontime calculations
    std::chrono::high_resolution_clock::time_point reactionStart;
    double reactionTime[2] = {0};
    double totalReactionTime[MAX_GAME_USERS] = {0};

    std::cout << "[Game started] " << user1->name << " vs " << user2->name << std::endl;
    while (game->playState == GameState::GOING && game->users[0] != nullptr && game->users[1] != nullptr)
    {        
        //Game has ended if one of the player has reached 3
        if (score[0] == 3 || score[1] == 3)
        {
            game->playState = GameState::ENDED;
            std::string gameEnd = "[Game ended] " + user1->name + " vs " + user2->name + " | Score: " +
                                  std::to_string(score[0]) + "-" + std::to_string(score[1]);

            std::cout << gameEnd << std::endl;
            SendSpectatorMSG(game, gameEnd + '\n');

            std::string scoreRes = std::to_string(score[0]) + "-" + std::to_string(score[1]);
            std::string winner = "[Game ended] You won! Score: " + scoreRes + '\n';
            std::string loser = "[Game ended] You lost! Score: " + scoreRes + '\n';

            //Player1 won
            if (score[0] > score[1])
            {
                SendMSG(user1->socket, winner);
                SendMSG(user2->socket, loser);
            }
            //Player2 won
            else
            {
                SendMSG(user1->socket, loser);
                SendMSG(user2->socket, winner);
            }

            //Calculate the average respons times
            double finalReactTime[MAX_GAME_USERS] = {0};
            for (int i = 0; i < MAX_GAME_USERS; i++)
            {
                finalReactTime[i] = totalReactionTime[i] / round;
            }
            std::cout << "[React time] " << user1->name << ": " << finalReactTime[0] << "s | " << user2->name << ": " << finalReactTime[1] << "s" << std::endl;

            //Saving down the score to highscore table if the time was low enough
            std::vector<HighscoreData> highscoreData;
            highscoreData.push_back({user1->name, finalReactTime[0]});
            highscoreData.push_back({user2->name, finalReactTime[1]});
            SaveToHighscore(highscoreData);
        }
        //Keep going as long as none has won
        else
        {
            //Current time
            auto timeNow = std::chrono::high_resolution_clock::now();
            //Time for the countdown
            std::chrono::duration<double> roundDiff = timeNow - roundStart;

            //1 second in
            if (roundDiff.count() >= 1 && !phase[0])
            {
                std::string firstMSG = "\nRound: " + std::to_string(round) + " | ";
                firstMSG.append(user1->name + ": " + std::to_string(score[0]) + " - ");
                firstMSG.append(user2->name + ": " + std::to_string(score[1]) + '\n');
                firstMSG.append("3 seconds until start");
                SendGameMSG(game, firstMSG);
                phase[0] = true;
            }

            //2 seconds in
            else if (roundDiff.count() >= 2 && !phase[1])
            {
                SendGameMSG(game, "2 seconds until start");
                phase[1] = true;
            }

            //3 seconds in
            else if (roundDiff.count() >= 3 && !phase[2])
            {
                SendGameMSG(game, "1 second until start");
                phase[2] = true;
            }

            //4 seconds in
            else if (roundDiff.count() >= 4)
            {
                //First time
                if (!phase[3])
                {
                    //Send only to players
                    SendGamePlayerMSG(game, "Please select an option:\n1. Rock\n2. Paper\n3. Scissor");
                    phase[3] = true;

                    //Clear all the messages that has been sent previously
                    ClearMSGQueue(user1->msgBuffer);
                    ClearMSGQueue(user2->msgBuffer);

                    //Start timer of the reactiontime
                    reactionStart = std::chrono::high_resolution_clock::now();
                }
                else
                {
                    std::chrono::duration<double> reactionDiff = timeNow - reactionStart;

                    //We got 2 seconds to respond to the server
                    if (reactionDiff.count() <= MAX_RESPONSTIME)
                    {
                        std::string p1MSG = GetMSG(user1->msgBuffer, true);
                        if (p1MSG != "")
                        {
                            input[0] = p1MSG;
                            //Calculate the responstime
                            std::chrono::duration<double> reaction = timeNow - reactionStart;
                            reactionTime[0] = reaction.count();
                        }
                        std::string p2MSG = GetMSG(user2->msgBuffer, true);
                        if (p2MSG != "")
                        {
                            input[1] = p2MSG;
                            //Calculate the responstime
                            std::chrono::duration<double> reaction = timeNow - reactionStart;
                            reactionTime[1] = reaction.count();
                        }
                    }
                    //When more than 2 seconds has passed
                    //We will check the reponses and see who won
                    else if (reactionDiff.count() > MAX_RESPONSTIME)
                    {
                        Winner winner = GameLogic(game, input);
                        switch (winner)
                        {
                        case Winner::PLAYER1:
                            score[0]++;
                            std::cout << "[Game] " << user1->name << "* vs " << user2->name << " | Round: "
                                      << round << " | Score: " << score[0] << " - " << score[1] << std::endl;
                            if (score[0] != 3)
                            {
                                round++;
                                SendMSG(user1->socket, "[WIN] You won this round!");
                                SendMSG(user2->socket, "[LOSE] You lost this round...");
                                SendSpectatorMSG(game, user1->name + " won this round!");
                            }
                            break;
                        case Winner::PLAYER2:
                            score[1]++;
                            std::cout << "[Game] " << user1->name << " vs " << user2->name << "* | Round: "
                                      << round << " | Score: " << score[0] << " - " << score[1] << std::endl;
                            if (score[1] != 3)
                            {
                                round++;
                                SendMSG(user1->socket, "[LOSE] You lost this round...");
                                SendMSG(user2->socket, "[WIN] You won this round!");
                                SendSpectatorMSG(game, user2->name + " won this round!");   
                            }
                            break;
                        case Winner::NONE:
                            SendGameMSG(game, "[DRAW] This round was a draw. Restarting round...");
                            std::cout << "[Game] " << user1->name << " vs " << user2->name << " | Round: "
                                      << round << " | Score: " << score[0] << " - " << score[1] << std::endl;
                            break;
                        }

                        //We only add to total reactiontime when one of the players won that round
                        if (winner != Winner::NONE)
                        {
                            for (int i = 0; i < MAX_GAME_USERS; i++)
                            {
                                //If the player did not answer we set reactiontime to 2 seconds
                                if (game->choice[i] == RPSChoice::NONE)
                                    reactionTime[i] = MAX_RESPONSTIME;

                                //Add to the total time
                                totalReactionTime[i] += reactionTime[i];
                            }
                        }

                        //Reset
                        for (int i = 0; i < 4; i++)
                            phase[i] = false;
                        for (int i = 0; i < 2; i++)
                        {
                            input[i] = "";
                            reactionTime[i] = 0;
                        }
                        roundStart = std::chrono::high_resolution_clock::now();
                    }
                }
            }
        }
    }
    //--- Before quiting ---
    ClearMSGQueue(user1->msgBuffer);
    ClearMSGQueue(user2->msgBuffer);
}

Winner Server::GameLogic(Game* game, std::string msg[2])
{
    Winner winner = Winner::NONE;

    //Convert from what the players picked to type RPSChoice
    for (int u = 0; u < 2; u++)
    {
        std::stringstream ss(msg[u]);
        int ansInt;
        if (ss >> ansInt)
        {
            switch (ansInt)
            {
            case 1:
                game->choice[u] = RPSChoice::ROCK;
                break;
            case 2:
                game->choice[u] = RPSChoice::PAPER;
                break;
            case 3:
                game->choice[u] = RPSChoice::SCISSOR;
                break;
            default:
                game->choice[u] = RPSChoice::NONE;
                break;
            }
        }
        else
        {
            game->choice[u] = RPSChoice::NONE;
        }
    }

    //Print out the players options
    if (m_debugMode)
    {
        std::string p1Name = game->users[0]->name;
        std::string p2Name = game->users[1]->name;
        std::cout << "[DEBUG] " << p1Name << " picked: " << RPSChoiceText.at(game->choice[0]) << " | " 
                  << p2Name << " picked: " << RPSChoiceText.at(game->choice[1]) << std::endl; 
    }

    //Draw - same answer
    if (game->choice[0] == game->choice[1])
        winner = Winner::NONE;

    //Player1 did not answer while player2 did answer
    else if (game->choice[0] == RPSChoice::NONE && 
             game->choice[1] != RPSChoice::NONE)
        winner = Winner::PLAYER2;

    //Player2 did not answer while player1 did answer
    else if (game->choice[1] == RPSChoice::NONE && 
             game->choice[0] != RPSChoice::NONE)
        winner = Winner::PLAYER1;

    //Player1 picked rock
    else if (game->choice[0] == RPSChoice::ROCK)
    {
        //Player2 picked paper
        if (game->choice[1] == RPSChoice::PAPER)
            winner = Winner::PLAYER2;
        //Player2 picked scissor
        else if (game->choice[1] == RPSChoice::SCISSOR)
            winner = Winner::PLAYER1;
    }

    //Player1 picked paper
    else if (game->choice[0] == RPSChoice::PAPER)
    {
        //Player2 picked rock
        if (game->choice[1] == RPSChoice::ROCK)
            winner = Winner::PLAYER1;
        //Player2 picked scissor
        else if (game->choice[1] == RPSChoice::SCISSOR)
            winner = Winner::PLAYER2;
    }

    //Player1 picked scissor
    else if (game->choice[0] == RPSChoice::SCISSOR)
    {
        //Player2 picked rock
        if (game->choice[1] == RPSChoice::ROCK)
            winner = Winner::PLAYER2;
        //Player2 picked paper
        else if (game->choice[1] == RPSChoice::PAPER)
            winner = Winner::PLAYER1;
    }

    return winner;
}

void Server::RemoveUserFromGame(const std::string& name)
{
    if (m_users.find(name) != m_users.end())
    {
        bool foundUser = false;
        const User* user = m_users[name];
        int gameID = GetGameNr(name);
        if (gameID != -1)
        {
            //Search through every player
            for (int u = 0; u < MAX_GAME_USERS && !foundUser; u++)
            {
                if (m_games[gameID]->users[u] == user)
                {
                    m_games[gameID]->users[u] = nullptr;
                    foundUser = true;
                }
            }

            //Search through every spectator
            for (int s = 0; s < MAX_GAME_SPECTATORS && !foundUser; s++)
            {
                if (m_games[gameID]->spectators[s] == user)
                {
                    m_games[gameID]->spectators[s] = nullptr;
                    foundUser = true;
                }
            }
        }
    }
}

void Server::CloseGame(const std::string& leavingPlayer)
{
    if (m_users.find(leavingPlayer) != m_users.end())
    {
        int gameID = GetGameNr(leavingPlayer);
        if (gameID != -1)
        {
            Game* game = m_games[gameID];
            User* user1 = game->users[0];
            User* user2 = game->users[1];
            
            std::string p1name = "...";
            std::string p2name = "...";
            if (user1 != nullptr)
                p1name = user1->name;
            if (user2 != nullptr)
                p2name = user2->name;

            if (user1 != nullptr && user1->name == leavingPlayer)
            {
                if (user2 != nullptr)
                    SendMSG(user2->socket, leavingPlayer + " left the game...\n");
            }
            else if (user2 != nullptr && user2->name == leavingPlayer)
            {
                if (user1 != nullptr)
                    SendMSG(user1->socket, leavingPlayer + " left the game...\n");
            }
            
            std::string gameStatus = "";
            if (game->playState == GameState::GOING)
            {
                pthread_cancel(game->thread);
                gameStatus = "[Game ended] " + p1name + " vs " + p2name;
            }
            else if (game->playState == GameState::READY)
                gameStatus = "[Game closed down]";

            std::cout << gameStatus << std::endl;

            //Send back the users to menu and the reason
            for (int u = 0; u < MAX_GAME_USERS; u++)
            {
                if (game->users[u] != nullptr)
                {
                    SendMSG(game->users[u]->socket, gameStatus);
                    ReturnUserToMenu(game->users[u]);
                    game->users[u] = nullptr;
                }
            }

            //Send back the spectators to menu and the reason
            for (int s = 0; s < MAX_GAME_SPECTATORS; s++)
            {
                if (game->spectators[s] != nullptr)
                {
                    SendMSG(game->spectators[s]->socket, gameStatus);
                    ReturnUserToMenu(game->spectators[s]);
                    game->spectators[s] = nullptr;
                }
            }
    
            game->playState = GameState::ENDED;
            delete game;
            m_games.erase(m_games.begin() + gameID);

            if (m_debugMode)
            {
                std::cout << "Active games: " << m_games.size() << std::endl;
            }
        }
    }
    else
    {
        std::cout << "Player: " << leavingPlayer << " does not exist..." << std::endl;
    }
}

void Server::SendMenu(const std::string& name)
{
    SendMSG(m_users[name]->socket, "Please select:\n1. Play\n2. Spectate\n3. Highscore\n0. Exit");

    //State change
    m_users[name]->state = PlayerState::MENU_RECV;
}

void Server::RecvMenu(const std::string& username, const std::string& msg)
{
    std::stringstream ss(msg);
    int option;
    if (ss >> option)
    {
        switch (option)
        {
        //Show active games or create one
        case 1:
            m_users[username]->type = UserType::PLAYER;
            m_users[username]->state = PlayerState::GAMEQUEUE_SEND;
            break;
        //Spectate a game
        case 2:
            m_users[username]->type = UserType::SPECTATOR;
            m_users[username]->state = PlayerState::GAMEQUEUE_SEND;
            break;
        //Highscore
        case 3:
            m_users[username]->state = PlayerState::HIGHSCORE_SEND;
            break;
        //Quit
        case 0:
            m_users[username]->state = PlayerState::QUIT;
            break;
        default:
            break;
        };
    }
    else
    {
        SendMSG(m_users[username]->socket, KEYS.at(Keyword::ERROR) + "MSG was not a number...");
    }
}

void Server::ReturnUserToMenu(User* user)
{
    user->readyToStart = false;
    user->state = PlayerState::MENU_SEND;
    user->type = UserType::NONE;
    ClearMSGQueue(user->msgBuffer);
}

void Server::SendGameQueue(User* user)
{
    //If no games exists and the user is a spectator
    //we will return this player to menu
    if (user->type == UserType::SPECTATOR && m_games.size() == 0)
    {
        SendMSG(user->socket, "No games to spectate...\n");
        ReturnUserToMenu(user);
    }
    else
    {
        std::string gameQueue = "--- Games ---\n";

        //Create a new game or join a created game
        if (user->type == UserType::PLAYER)
            gameQueue.append("1. Create a new game\n");
        
        for (int g = 0; g < m_games.size(); g++)
        {
            Game *game = m_games[g];
            int nrPlayers = GetNrOfUsers(game);

            //Layout: 1. Player vs Player2 | 2/2
            gameQueue.append(std::to_string(g + 2) + ". ");
            for (int p = 0; p < nrPlayers; p++)
            {
                gameQueue.append(game->users[p]->name);
                if (p != nrPlayers - 1)
                    gameQueue.append(" vs ");
            }
            gameQueue.append(" | " + std::to_string(nrPlayers) + "/" + std::to_string(MAX_GAME_USERS) + '\n');
        }
        gameQueue.append("0. Return to menu");

        SendMSG(user->socket, gameQueue);
        user->state = PlayerState::GAMEQUEUE_RECV;
    }
}

void Server::RecvGameQueue(User* user, const std::string& msg)
{
    //Avoids to do stuff with players that is already in a game
    if (GetGameNr(user->name) == -1)
    {
        std::stringstream ss(msg);
        int option;
        if (ss >> option)
        {
            //Return to menu
            if (option == 0)
                ReturnUserToMenu(user);

            int nrOfGames = (int)m_games.size();

            if (user->type == UserType::SPECTATOR)
            {
                //Layout: Only shows active games
                if (option > 1 && option <= (nrOfGames+1))
                {
                    int gameNr = option - 2;
                    Game *game = m_games[gameNr];
                    int nrOfSpectators = GetNrOfSpectators(game);
                    if (nrOfSpectators < MAX_GAME_USERS)
                    {
                        game->spectators[nrOfSpectators] = user;
                        user->state = PlayerState::INGAME;
                        
                        std::string p1 = "<NONE>";
                        std::string p2 = "<NONE>";
                        if (game->users[0] != nullptr)
                            p1 = game->users[0]->name;
                        if (game->users[1] != nullptr)
                            p2 = game->users[1]->name;

                        std::string specMSG = "You are now spectating game " + std::to_string(gameNr) + " between " + p1 + " vs " + p2;
                        specMSG.append("\nPress 'any key' to return to menu");
                        SendMSG(user->socket, specMSG);
                    }
                    else
                    {
                        SendMSG(user->socket, "Game was full... Max spectators " + std::to_string(MAX_GAME_SPECTATORS));
                    }
                }
                else
                {
                    SendMSG(user->socket, "Wrong input...");
                }
            }
            else if (user->type == UserType::PLAYER)
            {
                //Layout: Show create new game and active games
            
                //Creating a new game
                if (option == 1)
                {
                    Game *newGame = new Game();
                    newGame->users[0] = user;
                    m_games.push_back(newGame);
                    SendMSG(user->socket, "New game created! Waiting for player");
                }
                //Joining an existing game
                else if (option > 1 && option <= (nrOfGames+1) )
                {
                    int gameNr = option - 2;
                    Game *game = m_games[gameNr];

                    int nrOfUsers = GetNrOfUsers(game);
                    if (nrOfUsers < MAX_GAME_USERS)
                    {
                        if (game->playState == GameState::READY)
                        {
                            game->users[nrOfUsers] = user;

                            //Game is ready when game is full
                            if (game->users[0] != nullptr && game->users[1] != nullptr)
                            {
                                for (int p = 0; p < MAX_GAME_USERS; p++)
                                {
                                    if (game->users[p] != nullptr)
                                    {
                                        SendMSG(game->users[p]->socket, "\n---The game is now ready!---\nPress '1' to accept or '0' to leave");
                                        game->users[p]->state = PlayerState::GAMEQUEUE_READY;
                                    }
                                }
                            }
                        }
                        else
                        {
                            SendMSG(user->socket, "Game is already going on...");
                        }
                    }
                    else
                    {
                        SendMSG(user->socket, "Game was full...");
                    }
                }
                else
                {
                    SendMSG(user->socket, KEYS.at(Keyword::ERROR)+"Wrong input...");
                }
            }
        }
        else
        {
            SendMSG(user->socket, KEYS.at(Keyword::ERROR)+"Only numbers are acceptable");
        }
    }
    else
    {
        SendMSG(user->socket, "You are already in a game");
    }
}

void Server::RecvGameQueueReady(const std::string& name, const std::string& msg)
{
    std::stringstream ss(msg);
    int option;
    if (ss >> option)
    {
        switch (option)
        {
            case 1:
                int gameNr;
                gameNr = GetGameNr(name);
                if (gameNr != -1)
                {
                    m_users[name]->readyToStart = true;
                    SendMSG(m_users[name]->socket, "You are now ready!");
                    Game* game = m_games[gameNr]; 

                    //If all the players are ready we can start the game
                    if (game->users[0] != nullptr &&
                        game->users[1] != nullptr)
                    {
                        if (game->users[0]->readyToStart &&
                            game->users[1]->readyToStart)
                        {
                            game->users[0]->state = PlayerState::INGAME;
                            game->users[1]->state = PlayerState::INGAME;

                            //Start game thread
                            game->server = this;    //Point to the serverclass
                            if (pthread_create(&game->thread, nullptr, PlayGameThread, (void*)game) != 0)
                                std::cout << "Pthread_create() failed..." << std::endl;        
                        }
                    }                    
                }
                else
                {
                    SendMSG(m_users[name]->socket, KEYS.at(Keyword::ERROR) + "User was not in the game...");
                }
                break;
            case 0:
                int gameID;
                gameID = GetGameNr(name);
                if (gameID != -1)
                {
                    Game* game = m_games[gameID];
                    for (int u = 0; u < MAX_GAME_USERS; u++)
                    {
                        if (game->users[u] != nullptr)
                            ReturnUserToMenu(game->users[u]);
                    }
                    for (int s = 0; s < MAX_GAME_SPECTATORS; s++)
                    {
                        if (game->spectators[s] != nullptr)
                            ReturnUserToMenu(game->spectators[s]);
                    }

                    delete game;
                    m_games.erase(m_games.begin() + gameID);
                }
                break;
            default: 
                SendMSG(m_users[name]->socket, "Wrong input... Try: '0' or '1'");
                break;
        }
    }
    else
    {
        std::cout << name << " sent something that was not a number..." << std::endl;
    }
}

std::vector<HighscoreData> Server::ReadHighFile(const std::string& filename)
{
    std::vector<HighscoreData> highscoreTable;

    std::ifstream readFile;
    readFile.open(filename);
    if (readFile.is_open())
    {
        //Go through the file
        std::string line = "";
        while(getline(readFile, line))
        {
            size_t delimiter = line.find('|');
            if (delimiter != std::numeric_limits<size_t>::max())
            {
                HighscoreData data;
                data.username = line.substr(0, delimiter);
                std::stringstream ss(line.substr(delimiter+1));
                if (ss >> data.responstime)
                {
                    highscoreTable.push_back(data);
                }
            }
        }
    }
    readFile.close();

    return highscoreTable;
}

void Server::SaveToHighscore(const std::vector<HighscoreData> inputData)
{
    //Read from the file if it exists
    std::vector<HighscoreData> highscoreTable = ReadHighFile("Highscore");

    //Insert the input and check with names in highscoretable
    for (int i = 0; i < inputData.size(); i++)
    {
        bool foundUser = false;

        //Search through the list after same name
        for (int h = 0; h < highscoreTable.size() && !foundUser; h++)
        {
            //Same name
            if (inputData[i].username == highscoreTable[h].username)
            {
                //Compare the respons times
                foundUser = true;
                if (inputData[i].responstime < highscoreTable[h].responstime)
                {
                    highscoreTable[h] = inputData[i];
                }
            }
        }

        if (!foundUser)
            highscoreTable.push_back(inputData[i]);
    }

    //Sort the table after the lowest respons time
    std::sort(highscoreTable.begin(), highscoreTable.end(), std::less<HighscoreData>());

    //Write the highscore table to file.
    //Trunc clears the file before writing to it
    //Limits it to MAX_HIGHSCORE_USERS
    std::ofstream writeFile;
    writeFile.open("Highscore", std::ofstream::out | std::ofstream::trunc);

    bool keepWriting = true;
    for (int i = 0; i < highscoreTable.size() && keepWriting; i++)
    {
        if (i < MAX_HIGHSCORE_USERS)
            writeFile << highscoreTable[i].username << "|" << highscoreTable[i].responstime << std::endl;
        else
            keepWriting = false;
    }

    writeFile.close();
    highscoreTable.clear();
}

void Server::SendHighscore(User* user)
{
    //Read the list into a vector
    std::vector<HighscoreData> highscoreTable = ReadHighFile("Highscore");

    //Sorting if needed
    std::sort(highscoreTable.begin(), highscoreTable.end(), std::less<HighscoreData>());
    
    std::string highscoreString = "-------------- Top " + std::to_string(MAX_HIGHSCORE_USERS) + " --------------\n";
    highscoreString.append("Place\t    Name\tResponstime\n");

    bool keepAdding = true;
    for (int i = 0; i < highscoreTable.size() && keepAdding; i++)
    {
        if (i < MAX_HIGHSCORE_USERS)
        {
            highscoreString.append("  " + std::to_string(i+1) + '\t');
            //Depending on the length of the username add extra space after
            int nameLength = highscoreTable[i].username.length();
            int difference = MAXNAMELENGTH - nameLength;
            int firstHalf = difference / 2;
            int secondHalf = difference - firstHalf;

            //Center the name
            for (int f = 0; f < firstHalf; f++)
                highscoreString.append(" ");
            highscoreString.append(highscoreTable[i].username);
            for (int s = 0; s < secondHalf; s++)
                highscoreString.append(" ");

            highscoreString.append('\t' + std::to_string(highscoreTable[i].responstime) + "s\n");
        }
        else
            keepAdding = false;
    }
    highscoreString.append("-----------------------------------");
    highscoreString.append("\nPress 'any key' to go back to menu");

    SendMSG(user->socket, highscoreString);
    user->state = PlayerState::HIGHSCORE_RECV;
}

void Server::SendGameMSG(const Game* game, std::string msg)
{
    SendGamePlayerMSG(game, msg);
    SendSpectatorMSG(game, msg);
}

void Server::SendGamePlayerMSG(const Game* game, std::string msg)
{
    msg.append(END);

    //Send message to every player in the game
    for (int u = 0; u < MAX_GAME_USERS; u++)
    {
        if (game->users[u] != nullptr)
        {
            if (send(game->users[u]->socket, &msg[0], msg.length(), 0) < 0)
                std::cout << "Failed to send message: '" << msg << "' to: " << game->users[u]->name << "..." << std::endl;
        }
    }
}

void Server::SendSpectatorMSG(const Game* game, std::string msg)
{
    msg.append(END);

    //Send message to every player in the spectator room
    for (int s = 0; s < MAX_GAME_SPECTATORS; s++)
    {
        if (game->spectators[s] != nullptr)
        {
            if (send(game->spectators[s]->socket, &msg[0], msg.length(), 0) < 0)
                std::cout << "Failed to send message: '" << msg << "' to: " << game->spectators[s]->name << "..." << std::endl;
        }
    }
}

void Server::ClearLeaveQueue()
{
    //Remove users that have choosen to leave the server
    for (int i = 0; i < m_leaveQueue.size(); i++)
    {
        std::string name = m_leaveQueue[i];
        int socket = m_users[name]->socket;
        
        if (m_users[name]->type == UserType::PLAYER)
            CloseGame(name);
        else if (m_users[name]->type == UserType::SPECTATOR)
            RemoveUserFromGame(name);
        else 
            SendMSG(socket, KEYS.at(Keyword::QUIT));

        std::cout << name << " left the server..." << std::endl;

        //Closing down the socket and remove the user from the list
        close(socket);
        delete m_users[name];
        m_users.erase(name);
    }
    m_leaveQueue.clear();
}

void Server::SignumCall(int signum)
{
    sigInt = signum;  
}

bool Server::Setup(std::string address, std::string port)
{
    struct addrinfo hints = {0};
    hints.ai_family = AF_UNSPEC;        //IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;    //TCP protocol

    struct addrinfo* results = nullptr;
    struct addrinfo* current = nullptr;

    int error = getaddrinfo(address.c_str(), port.c_str(), &hints, &results);
    if (error != 0)
    {
        std::cout << "GetAddrInfo failed... ERROR Code: " << gai_strerror(error) << std::endl;
        return false;
    }

    struct sockaddr_storage* serverAddr = nullptr;

    //Got through all the address that can fit
    for (current = results; current != nullptr; current->ai_next)
    {
        m_masterSocket = socket(current->ai_family, 
                                current->ai_socktype, 
                                current->ai_protocol);

        if (m_masterSocket < 0)
            continue;

        //Successfully binded the socket if it was lower than 0
        if (bind(m_masterSocket, current->ai_addr, current->ai_addrlen) == 0)
        {
            //Saves down the address
            serverAddr = (sockaddr_storage*)current->ai_addr;
            break;
        }
        close(m_masterSocket);
    }
    freeaddrinfo(results);

    //Use same address and port for every connection
    int yes = 1;
    if (setsockopt(m_masterSocket, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &yes, sizeof(yes)) < 0)
    {
        std::cout << "Failed to setup socket options..." << std::endl;
        return false;
    }

    if (listen(m_masterSocket, BACKLOG) < 0)
    {
        std::cout << "Failed to setup listen..." << std::endl;
        return false;
    }

    std::string finalIP = "";
    std::string finalPort = "";
    if (!GetIpAndPort(*serverAddr, finalIP, finalPort))
    {
        return false;
    }
    std::cout << "Listening on address: " << finalIP << " and port: " << finalPort << std::endl;

    return true;
}

void Server::Run()
{
    fd_set readfds;
    //Let us know when we press Ctrl+C
    signal(SIGINT, SignumCall);

    while (m_isRunning)
    {
        /*
            Select to see if we got anything new from any user
        */
        int maxFd = PrepareSet(readfds);
        //10ms wait time
        struct timeval tv = {0, 10000};

        switch (select(maxFd + 1, &readfds, nullptr, nullptr, &tv))
        {
            case -1:
                //If we got a sigint, we shall close down
                if (sigInt == SIGINT)
                {
                    std::cout << "\nClosing down server" << std::endl;
                    m_isRunning = false;                    
                }
                break;
            case 0:
                break;
            default:
                //Mastersocket, checks incoming new users
                if (FD_ISSET(m_masterSocket, &readfds))
                    CheckIncoming();

                //Go through all the active users
                for (auto& user : m_users)
                {
                    //The user has sent a message that we have to read
                    //and buffer to the message queue for that user
                    if (FD_ISSET(user.second->socket, &readfds))
                    {
                        if (!RecvMSG(user.second->socket, user.second->msgBuffer))
                            user.second->state = PlayerState::QUIT;
                    }
                }
                break;
        }
        CloseSet(readfds);

        /*
            Go through every user an read their messages
            Check their states and do what is needed for them
        */
        for (auto& user: m_users)
        {
            std::string clientMSG = GetMSG(user.second->msgBuffer);

            if (clientMSG != "")
            {
                Keyword key = GetKeyword(clientMSG);
                switch (key)
                {
                case Keyword::QUIT:
                    EraseLastMSG(user.second->msgBuffer);
                    user.second->state = PlayerState::QUIT;
                    break;
                case Keyword::ERROR:
                    EraseLastMSG(user.second->msgBuffer);
                    std::cout << "ERROR: " << GetKeyMSG(clientMSG, Keyword::ERROR) << std::endl;
                    break;
                default:
                    break;
                }

                //Recv states
                switch (user.second->state)
                {
                    case PlayerState::MENU_RECV:
                        EraseLastMSG(user.second->msgBuffer);
                        RecvMenu(user.first, clientMSG);
                        break;
                    case PlayerState::GAMEQUEUE_RECV:
                        EraseLastMSG(user.second->msgBuffer);
                        RecvGameQueue(user.second, clientMSG);
                        break;
                    case PlayerState::GAMEQUEUE_READY:
                        EraseLastMSG(user.second->msgBuffer);
                        RecvGameQueueReady(user.first, clientMSG);
                        break;
                    case PlayerState::HIGHSCORE_RECV:
                        ReturnUserToMenu(user.second);
                        break;
                    case PlayerState::INGAME:
                        //If a spectator in a game send a message
                        //We shall remove them from the game and
                        //return them to menu
                        if (user.second->type == UserType::SPECTATOR)
                        {
                            SendMSG(user.second->socket, "\nSending you back to menu\n");
                            EraseLastMSG(user.second->msgBuffer);
                            RemoveUserFromGame(user.second->name);
                            ReturnUserToMenu(user.second);
                        }
                        break;
                    default:
                        break;
                }
            }

            //Game states
            switch (user.second->state)
            {
            case PlayerState::QUIT:
                m_leaveQueue.push_back(user.first);
                break;
            case PlayerState::MENU_SEND:
                SendMenu(user.first);
                break;
            case PlayerState::GAMEQUEUE_SEND:
                SendGameQueue(user.second);
                break;
            case PlayerState::HIGHSCORE_SEND:
                SendHighscore(user.second);
                break;
            default:
                break;
            }
        }

        //Remove users from the unordered_map
        ClearLeaveQueue();

        //Check if a gamethread need to be joined
        for (int g = 0; g < m_games.size(); g++)
        {
            Game* game = m_games[g];
            if (game->playState == GameState::ENDED)
            {
                pthread_join(game->thread, nullptr);

                //Return every user to menu
                for (int u = 0; u < MAX_GAME_USERS; u++)
                {
                    if (game->users[u] != nullptr)
                        ReturnUserToMenu(game->users[u]);
                }
                //Return every spectator to menu
                for (int s = 0; s < MAX_GAME_SPECTATORS; s++)
                {
                    if (game->spectators[s] != nullptr)
                        ReturnUserToMenu(game->spectators[s]);
                }
                
                m_games.erase(m_games.begin() + g);
                delete game;

                //DEBUG MODE: Prints out that threads and games
                if (m_debugMode)
                {
                    std::cout << "[DEBUG] Current games: " << m_games.size() << std::endl;
                }
            }
        }
    }
}

/*
Start with: executableFile Address Port
Some examples that will work:
./sspd 127.0.0.1 6660
./sspd ::1 6660
./sspd localhost 6660
*/
int main(int argc, char *argv[])
{
    //Check that input was correct
    if (argc < 3 || argc > 3)
    {
        std::cout << "Wrong input... Expected: ./sspd Address Port" << std::endl;
        return 0;
    }

    Server server;
    
    //Setup the server with an address and port
    if (server.Setup(argv[1], argv[2]))
        server.Run();
    else
        std::cout << "Server setup failed... " << std::endl;

    return 0;
}
