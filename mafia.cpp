#include "players.hpp"
#include <algorithm>
#include <iostream>
#include <iterator>
#include <random>
#include <vector>

int 
main(int argc, char **argv) 
{
    std::srand(std::time(nullptr));

    int N, k;
    bool gamer, op_cl_info;
    char c_gamer, c_op_cl_info;

    printf("Enter the number of players -- N (N >= 6):\n");
    scanf("%d", &N);
    printf("Enter number -- k, which will determine the number of mafias according" 
            "to the formula N/k (k >= 2):\n");
    scanf("%d", &k);
    printf("Will you play?(y/n):\n");
    scanf(" %c", &c_gamer);
    printf("Open role announcement?(y/n):\n");
    scanf(" %c", &c_op_cl_info);

    gamer = c_gamer == 'y' ? true : false;
    op_cl_info = c_op_cl_info == 'y' ? true : false;

    std::cout << N << " " << k << " " << gamer << " " << op_cl_info << "\n";

    int mafia_count = N / k;
    if (mafia_count == 0)
        abort();

    std::vector<int> pers(N, 0);
    pers[0] = DOC; pers[1] = COMA; pers[2] = MANA; pers[3] = KILLER; pers[4] = NINJA; pers[5] = BULL;
    for (int i = 6; i < 3 + mafia_count; ++i) 
        pers[i] = MAFIA;
    for (int i = 3 + mafia_count; i < N; ++i)
        pers[i] = CIVILIAN;

    std::random_device rd;
    std::mt19937 g(rd());
 
    std::shuffle(pers.begin(), pers.end(), g);

    Shared_ptr<Data> data = new Data(N, mafia_count);
    Shared_ptr<Coma_to_host> coma_to_host = new Coma_to_host();
    Shared_ptr<Mana_to_host> mana_to_host = new Mana_to_host();
    Shared_ptr<Doc_to_host> doc_to_host = new Doc_to_host();
    Shared_ptr<Killer_to_host> killer_to_host = new Killer_to_host();
    Shared_ptr<Mafia_privat> mafia_privat = new Mafia_privat(mafia_count);

    std::vector<Player*> players_struct;
    Host host = Host(
        data, 
        coma_to_host, 
        mana_to_host, 
        doc_to_host,
        killer_to_host,
        mafia_privat, 
        pers,
        op_cl_info
    );

    std::set<int> maf_bro;

    for (int i = 0; i < N; ++i) 
        if (pers[i] == MAFIA || pers[i] == KILLER || pers[i] == NINJA || pers[i] == BULL) {
            maf_bro.insert(i);
        }

    std::set<int> maf_num = {MAFIA, KILLER, NINJA, BULL};

    if (gamer) {
        int random_number = std::experimental::randint(0, int(N-1));

        std::cout << "Your number is "<< random_number << "\n";
        std::cout << "You are " << num_to_role[pers[random_number]] << "\n";

        if (maf_num.count(pers[random_number])) {
            std::cout << "Your maf bro: \n";

            for (auto i : maf_bro) 
                if (i != random_number)
                    std::cout << i << " ";

            std::cout << "\n";
            std::cout.flush();
        }

        for (int i = 0; i < N; ++i) {
            if (i == random_number) {
                switch (pers[i]) {
                    case CIVILIAN:
                        players_struct.push_back(static_cast<Player*>(new Civilian_cmd(i, data)));
                        break;
                    case DOC:
                        players_struct.push_back(static_cast<Player*>(new Doc_cmd(i, data, doc_to_host)));
                        break;
                    case COMA:
                        players_struct.push_back(static_cast<Player*>(new Coma_cmd(i, data, coma_to_host)));
                        break;
                    case MANA:
                        players_struct.push_back(static_cast<Player*>(new Mana_cmd(i, data, mana_to_host)));
                        break;
                    case MAFIA:
                        players_struct.push_back(static_cast<Player*>(new Mafia_cmd(i, data, mafia_privat, maf_bro)));
                        break;
                    case KILLER:
                        players_struct.push_back(static_cast<Player*>(new Killer_cmd(i, data, mafia_privat, maf_bro,
                                                                                     killer_to_host)));
                        break;
                    case NINJA:
                        players_struct.push_back(static_cast<Player*>(new Mafia_cmd(i, data, mafia_privat, maf_bro)));
                        break;                
                    case BULL:
                        players_struct.push_back(static_cast<Player*>(new Mafia_cmd(i, data, mafia_privat, maf_bro)));
                        break;
                }
            } else {
                switch (pers[i]) {
                    case CIVILIAN:
                        players_struct.push_back(static_cast<Player*>(new Civilian(i, data)));
                        break;
                    case DOC:
                        players_struct.push_back(static_cast<Player*>(new Doc(i, data, doc_to_host)));
                        break;
                    case COMA:
                        players_struct.push_back(static_cast<Player*>(new Coma(i, data, coma_to_host)));
                        break;
                    case MANA:
                        players_struct.push_back(static_cast<Player*>(new Mana(i, data, mana_to_host)));
                        break;
                    case MAFIA:
                        players_struct.push_back(static_cast<Player*>(new Mafia(i, data, mafia_privat, maf_bro)));
                        break;
                    case KILLER:
                        players_struct.push_back(static_cast<Player*>(new Killer(i, data, mafia_privat, maf_bro,
                                                                                     killer_to_host)));
                        break;
                    case NINJA:
                        players_struct.push_back(static_cast<Player*>(new Mafia(i, data, mafia_privat, maf_bro)));
                        break;                
                    case BULL:
                        players_struct.push_back(static_cast<Player*>(new Mafia(i, data, mafia_privat, maf_bro)));
                        break;
                }
            }
        }
    } else {
        for (int i = 0; i < N; ++i) {
            switch (pers[i]) {
                case CIVILIAN:
                    players_struct.push_back(static_cast<Player*>(new Civilian(i, data)));
                    break;
                case DOC:
                    players_struct.push_back(static_cast<Player*>(new Doc(i, data, doc_to_host)));
                    break;
                case COMA:
                    players_struct.push_back(static_cast<Player*>(new Coma(i, data, coma_to_host)));
                    break;
                case MANA:
                    players_struct.push_back(static_cast<Player*>(new Mana(i, data, mana_to_host)));
                    break;
                case MAFIA:
                    players_struct.push_back(static_cast<Player*>(new Mafia(i, data, mafia_privat, maf_bro)));
                    break;
                case KILLER:
                    players_struct.push_back(static_cast<Player*>(new Killer(i, data, mafia_privat, maf_bro,
                                                                                 killer_to_host)));
                    break;
                case NINJA:
                    players_struct.push_back(static_cast<Player*>(new Mafia(i, data, mafia_privat, maf_bro)));
                    break;                
                case BULL:
                    players_struct.push_back(static_cast<Player*>(new Mafia(i, data, mafia_privat, maf_bro)));
                    break;
            }
        }
    }

    std::vector<std::thread> t;

    std::thread th{&Host::host_loop, host};

    for (int i = 0; i < N; ++i) 
        t.push_back(std::thread {&Player::game_loop, players_struct[i]});

    th.join();

    for (int i = 0; i < N; ++i)
        t[i].join();
}
