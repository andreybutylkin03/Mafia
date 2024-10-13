#include <cstdlib>
#include <ctime>
#include <experimental/random>
#include <mutex>
#include <barrier>
#include <memory>
#include <future>
#include <queue>
#include <set>
#include <syncstream>
#include <map>

#include "shared_ptr.hpp"

enum Roles
{
    CIVILIAN,
    DOC, 
    COMA,
    MANA,
    MAFIA,
};

std::map<int, std::string> num_to_role = {
    {0, "CIVILLIAN"}, 
    {1, "DOC"},
    {2, "COMA"},
    {3, "MANA"},
    {4, "MAFIA"}
};

struct Data
{
    int N_;
    int mafia_count_;
    std::vector<bool> is_live_;
    std::mutex *mut_state_;
    std::mutex *mut_theme_;
    int theme_;
    std::vector<int> vote_list_;
    std::mutex *mut_vote_;
    std::barrier<> *bar_vote_;
    std::barrier<> *bar_res_d_;
    std::barrier<> *bar_res_n_;

    Data (const int &N, const int &mafia_count) : 
        N_(N), 
        mafia_count_(mafia_count)
    {
        is_live_.resize(N_, 1);
        vote_list_.resize(N_, -1);
        theme_ = -1;
        bar_vote_ = new std::barrier<>(N_ + 1);
        bar_res_d_ = new std::barrier<>(N_ + 1);
        bar_res_n_ = new std::barrier<>(N_ + 1);
        mut_state_ = new std::mutex;
        mut_theme_ = new std::mutex;
        mut_vote_ = new std::mutex;
    }
};


struct Coma_to_host
{
    std::barrier<> *bar_q_c_;
    std::barrier<> *bar_a_h_;
    bool type_q_;
    int q_;
    bool ans_; // 1 - maf, 0 - civ

    Coma_to_host () {
        bar_q_c_ = new std::barrier<>(2);
        bar_a_h_ = new std::barrier<>(2);
    }
};

struct Mana_to_host
{
    std::barrier<> *bar_q_c_;
    std::barrier<> *bar_a_h_;
    int q_;

    Mana_to_host() {
        bar_q_c_ = new std::barrier<>(2);
        bar_a_h_ = new std::barrier<>(2);
    }
};

struct Doc_to_host
{
    std::barrier<> *bar_q_c_;
    std::barrier<> *bar_a_h_;
    int q_;

    Doc_to_host () {
        bar_q_c_ = new std::barrier<>(2);
        bar_a_h_ = new std::barrier<>(2);
    }
};

struct Mafia_privat
{
    int mafia_count_;
    std::multiset<int> target_;
    std::set<int> s_target_;
    int tar_;
    std::mutex *mut_tar_;
    std::barrier<> *bar_maf_vote_;
    std::barrier<> *bar_maf_host_;

    Mafia_privat (const int &mafia_count) :
        mafia_count_(mafia_count) 
    {
        bar_maf_vote_ = new std::barrier<>(mafia_count_);
        bar_maf_host_ = new std::barrier<>(mafia_count_+1);
        tar_ = -1;
        mut_tar_ = new std::mutex;
    }

    void mafia_choice (void) {
        std::lock_guard<std::mutex> lg{*mut_tar_};
        if (tar_ == -1) {
            int count = 0;
            for (auto i : s_target_) {
                if (int(target_.count(i)) > count) {
                    count = target_.count(i);
                    tar_ = i;
                }
            }
        }
    }
};

class Host
{
    Shared_ptr<Data> host_data_;
    Shared_ptr<Coma_to_host> host_coma_to_host_;
    Shared_ptr<Mana_to_host> host_mana_to_host_;
    Shared_ptr<Doc_to_host> host_doc_to_host_;
    Shared_ptr<Mafia_privat> host_mafia_privat_;
    std::vector<int> role_for_num_;
    std::shared_future<int> f_doc_;
    std::shared_future<int> f_mana_;
    bool op_cl_info_;
    std::set<int> num_civ_;
    int num_doc_{-1};
    int num_coma_{-1};
    int num_mana_{-1};
    std::set<int> num_mafia_;
    std::set<int> now_live_;

public:
    Host(Shared_ptr<Data> &host_data, 
        Shared_ptr<Coma_to_host> &host_coma_to_host, 
        Shared_ptr<Mana_to_host> &host_mana_to_host, 
        Shared_ptr<Doc_to_host> &host_doc_to_host, 
        Shared_ptr<Mafia_privat> &host_mafia_privat, 
        std::vector<int> &role_for_num, 
        std::shared_future<int> &f_doc, 
        std::shared_future<int> &f_mana, 
        bool op_cl_info)
        :
        host_data_(host_data), 
        host_coma_to_host_(host_coma_to_host), 
        host_mana_to_host_(host_mana_to_host), 
        host_doc_to_host_(host_doc_to_host), 
        host_mafia_privat_(host_mafia_privat),
        role_for_num_(role_for_num),
        f_doc_(f_doc),
        f_mana_(f_mana),
        op_cl_info_(op_cl_info)
    {
        for (int i = 0; i < host_data_->N_; ++i)
            now_live_.insert(i);
    }

    int is_live(const std::set<int> &num) {
        int count = 0;
        
        for (auto i : num) 
            count += host_data_->is_live_[i];

        return count;
    }

    int state_game(void) { //0 - go, 1 - civ, 2 - maf, 3 - man
        int live_mafia = is_live(num_mafia_);
        int live_civ = is_live(num_civ_);
        int all_civ = live_civ + host_data_->is_live_[num_doc_] + 
            host_data_->is_live_[num_coma_] + host_data_->is_live_[num_mana_];

        if (live_mafia > all_civ)
            return 2;

        if (host_data_->is_live_[num_mana_] && live_mafia == all_civ)
            return 0;

        if (!host_data_->is_live_[num_mana_] && live_mafia == all_civ)
            return 2;

        if (!host_data_->is_live_[num_mana_]) {
            if (!live_mafia)
                return 1;
        } else {
            if (!live_mafia)
                return 3;
        }

        return 0;
    }

    void print_res(const int &state_res) {
        switch (state_res) {
            case 0:
                break;
            case 1:
                std::osyncstream(std::cout) << "Civillian win\n";
                break;
            case 2:
                std::osyncstream(std::cout) << "Mafia win\n";
                break;
            case 3:
                std::osyncstream(std::cout) << "Mana win\n";
                break;
        }

        for (int i = 0; i < (int)role_for_num_.size(); ++i) {
            switch (role_for_num_[i]) {
                case CIVILIAN:
                    std::osyncstream(std::cout) << i << " Civillian\n";
                    break;
                case DOC:
                    std::osyncstream(std::cout) << i << " Doc\n";
                    break;
                case COMA:
                    std::osyncstream(std::cout) << i << " Coma\n";
                    break;
                case MANA:
                    std::osyncstream(std::cout) << i << " Mana\n";
                    break;
                case MAFIA:
                    std::osyncstream(std::cout) << i << " Mafia\n";
                    break;
            }
        }
        std::cout.flush();
    }

    void vote_res(void) {
        std::vector<std::pair<int, int>> ms(host_data_->N_, {0, 0});
        for (int i = 0; i < host_data_->N_; ++i) 
            if (host_data_->vote_list_[i] != -1)
                ms[host_data_->vote_list_[i]] = {ms[host_data_->vote_list_[i]].first + 1, host_data_->vote_list_[i]};

        sort(ms.rbegin(), ms.rend());
        
        int r = 1;

        while (ms[0].first == ms[r].first)
            ++r;

        std::unique_lock<std::mutex> uls{*host_data_->mut_state_};
        if (r == 1) {
            host_data_->is_live_[ms[0].second] = 0;
            std::osyncstream(std::cout) << "Kick " << ms[0].second << "\n";
            std::cout.flush();
            now_live_.erase(ms[0].second);
        } else {
            int random_number = std::experimental::randint(0, int(1));

            if (random_number) {
                int target = std::experimental::randint(0, int(r-1));
                host_data_->is_live_[ms[target].second] = 0;
                std::osyncstream(std::cout) << "Kick " << ms[target].second << "\n";
                std::cout.flush();

                now_live_.erase(ms[target].second);
            } else {
                std::osyncstream(std::cout) << "No Kick today\n";
                std::cout.flush();            
            }
        }
        std::osyncstream(std::cout) <<  "\n";
        std::cout.flush();
    }

    void host_loop(void) {
        /*
    CIVILIAN,
    DOC, 
    COMA,
    MANA,
    MAFIA,

         */
        for (int i = 0; i < (int)role_for_num_.size(); ++i) {
            switch (role_for_num_[i]) {
                case CIVILIAN:
                    num_civ_.insert(i);
                    break;
                case DOC:
                    num_doc_ = i;
                    break;
                case COMA:
                    num_coma_ = i;
                    break;
                case MANA:
                    num_mana_ = i;
                    break;
                case MAFIA:
                    num_mafia_.insert(i);
                    break;
            }
        }

        int day = 1;

        while (true) {
            int target_doc = -1;
            int target_coma = -1;
            int target_mana = -1;
            int target_mafia = -1;
            std::osyncstream(std::cout) << "Day " << day << "\n\n";
            ++day;
            std::osyncstream(std::cout) << "Night" << "\n";
            std::cout.flush();
            std::unique_lock<std::mutex> ul{*host_data_->mut_theme_};
            
            host_data_->theme_ = 1;

            ul.unlock();

            if (host_data_->is_live_[num_mana_]) {
                host_mana_to_host_->bar_q_c_->arrive_and_wait();
                target_mana = host_mana_to_host_->q_; 
                host_mana_to_host_->bar_a_h_->arrive_and_wait();
            }

            if (host_data_->is_live_[num_coma_]) {
                host_coma_to_host_->bar_q_c_->arrive_and_wait();

                if (!host_coma_to_host_->type_q_) {
                    target_coma = host_coma_to_host_->q_;
                } else {
                    host_coma_to_host_->ans_ = 
                        role_for_num_[host_coma_to_host_->q_] == MAFIA ? 1 : 0;
                }

                host_coma_to_host_->bar_a_h_->arrive_and_wait();
            }

            //mafia
            {
                host_mafia_privat_->bar_maf_host_->arrive_and_wait();
                host_mafia_privat_->mafia_choice();

                target_mafia = host_mafia_privat_->tar_;
            }

            if (host_data_->is_live_[num_doc_]) {
                host_doc_to_host_->bar_q_c_->arrive_and_wait();
                target_doc = host_doc_to_host_->q_; 
                host_doc_to_host_->bar_a_h_->arrive_and_wait();
            }

            std::unique_lock<std::mutex> uls{*host_data_->mut_state_};

            if (target_mana != -1) {
                host_data_->is_live_[target_mana] = 0;
                now_live_.erase(target_mana);
            }

            if (target_mafia != -1) {
                host_data_->is_live_[target_mafia] = 0;
                now_live_.erase(target_mafia);
            }

            if (target_coma != -1) {
                host_data_->is_live_[target_coma] = 0;
                now_live_.erase(target_coma);
            }

            if (target_doc != -1) {
                host_data_->is_live_[target_doc] = 1;
                now_live_.insert(target_doc);
            }

            host_data_->bar_res_n_->arrive_and_wait();

            uls.unlock();

            std::osyncstream(std::cout) << "Night result" << "\n";
            std::cout.flush();

            if (op_cl_info_) {
                if (target_mana != -1) {
                    std::osyncstream(std::cout) << "Mana kill " << target_mana << "\n";
                }

                if (target_mafia != -1) {
                    std::osyncstream(std::cout) << "Mafia kill " << target_mafia << "\n";
                }

                if (target_coma != -1) {
                    std::osyncstream(std::cout) << "Coma kill " << target_coma << "\n";
                }

                if (target_doc != -1) {
                    std::osyncstream(std::cout) << "Doc save " << target_doc << "\n";
                }
            } else {
                std::set<int> kill_today{
                    target_mana, 
                    target_mafia,
                    target_coma
                };

                kill_today.erase(target_doc);

                if (kill_today.empty())
                    std::osyncstream(std::cout) << "No kill today\n";
                else {
                    std::osyncstream(std::cout) << "Today kill\n";

                    for (auto i : kill_today)
                        if (i != -1)
                            std::osyncstream(std::cout) << i << " ";
                    std::osyncstream(std::cout) << "\n";
                }
            }

            int state_res = state_game(); //0 - go, 1 - civ, 2 - maf, 3 - man

            if (state_res) {
                std::osyncstream(std::cout) << "\n";
                print_res(state_res);
                ul.lock();
                host_data_->theme_ = 2;
                ul.unlock();
                return;
            }

            std::osyncstream(std::cout) << "Now live\n";
            for (auto i : now_live_) 
                std::osyncstream(std::cout) << i << " ";
            std::osyncstream(std::cout) << "\n\n";

            std::osyncstream(std::cout) << "Day vote\n";

            std::cout.flush();

            ul.lock();
            host_data_->theme_ = 0;
            ul.unlock();

            host_data_->bar_vote_->arrive_and_wait();

            std::osyncstream(std::cout) << "Vote result\n";

            for (auto i : now_live_) 
                std::osyncstream(std::cout) << i << " ";
            std::osyncstream(std::cout) << "\n";

            for (auto i : now_live_) 
                std::osyncstream(std::cout) << host_data_->vote_list_[i] << " ";
            std::osyncstream(std::cout) << "\n";

            vote_res();

            std::osyncstream(std::cout) << "Now live\n";
            for (auto i : now_live_) 
                std::osyncstream(std::cout) << i << " ";
            std::osyncstream(std::cout) << "\n\n";

            std::cout.flush();

            host_data_->bar_res_d_->arrive_and_wait();

            state_res = state_game(); //0 - go, 1 - civ, 2 - maf, 3 - man

            if (state_res) {
                print_res(state_res);
                ul.lock();
                host_data_->theme_ = 2;
                ul.unlock();
                return;
            }

            //clear all
            
            for (int i = 0; i < host_data_->N_; ++i) 
                host_data_->vote_list_[i] = -1;

            host_mafia_privat_->target_.clear();
            host_mafia_privat_->s_target_.clear();
            host_mafia_privat_->tar_ = -1;
        }
    }

};

class Player
{
public:
    int num_;
    Shared_ptr<Data> data_;

    Player () = default;

    Player (const int &num, Shared_ptr<Data> &data) :
        num_(num),
        data_(data)
    {}

    virtual void act(void) {
    }
    virtual void vote(void) {
        int target = -1;

        while (true) {
            target = std::experimental::randint(0, int(data_->N_-1));
            
            if (data_->is_live_[target] && target != num_)
                break;
        }

        std::lock_guard<std::mutex> lg{*data_->mut_vote_};
        data_->vote_list_[num_] = target;
    } 

    virtual void act_after_die(void) {}

    void game_loop(void) {
        int self_theme = -1; //0 - day, 1 - night, 2 - end 
        while (true) {
            std::unique_lock<std::mutex> uls{*data_->mut_state_};
            if (!data_->is_live_[num_])
                break;

            uls.unlock();

            std::unique_lock<std::mutex> ul{*data_->mut_theme_};

            if (data_->theme_ == 0) {
                ul.unlock();
                if (self_theme == 0)
                    std::this_thread::yield();
                else {
                    vote();
                    auto ar_out = data_->bar_vote_->arrive();

                    data_->bar_res_d_->arrive_and_wait();
                }

                self_theme = 0;

            } else if (data_->theme_ == 1) {
                ul.unlock();
                if (self_theme == 1)
                    std::this_thread::yield();
                else {
                    act();

                    data_->bar_res_n_->arrive_and_wait();
                }

                self_theme = 1;

            } else if (data_->theme_ == 2)
                return;
        }

        while (true) {
            std::unique_lock<std::mutex> ul{*data_->mut_theme_};

            if (data_->theme_ == 0) {
                ul.unlock();
                if (self_theme == 0)
                    std::this_thread::yield();
                else {
                    auto ar_out = data_->bar_vote_->arrive();
                    data_->bar_res_d_->arrive_and_wait();
                }

                self_theme = 0;
            } else if (data_->theme_ == 1) {
                ul.unlock();
                if (self_theme == 1)
                    std::this_thread::yield();
                else {
                    act_after_die();
                    data_->bar_res_n_->arrive_and_wait();
                }

                self_theme = 1;
            } else if (data_->theme_ == 2)
                return;
        }
    }

    virtual ~Player() = default;
};

void vote_cmd(Shared_ptr<Data> &data_, const int &num_) {
    int target = -1;
    std::osyncstream(std::cout) << "Your choice:\n";
    std::cout.flush();

    while (true) {
        std::cin >> target; 
        
        if (data_->is_live_[target] && target != num_)
            break;
        else 
            std::osyncstream(std::cout) << "Wrong number, try again\n";
        std::cout.flush();
    }

    std::lock_guard<std::mutex> lg{*data_->mut_vote_};
    data_->vote_list_[num_] = target;
}

class Civilian : public Player
{
public:
    Civilian () = default;
    Civilian (const int &num, Shared_ptr<Data> &data) {
        num_ = num;
        data_ = data;
    }
};

class Civilian_cmd : public Civilian
{
public:
    Civilian_cmd (const int &num, Shared_ptr<Data> &data) {
        num_ = num;
        data_ = data;
    }

    void vote(void) override {
        vote_cmd(data_, num_);
    }
};

class Doc : public Player
{
public:
    int prev_safe_;
    Shared_ptr<Doc_to_host> doc_to_host_;

    Doc () = default;

    Doc (const int &num, Shared_ptr<Data> &data, Shared_ptr<Doc_to_host> &doc_to_host) {
        num_ = num;
        data_ = data;
        prev_safe_ = -1;
        doc_to_host_ = doc_to_host;
    }

    void act(void) override {
        int target = -1;

        while (true) {
            target = std::experimental::randint(0, int(data_->N_-1));
            
            if (data_->is_live_[target] && prev_safe_ != target) {
                prev_safe_ = target;
                break;
            }
        }

        doc_to_host_->q_ = target;

        doc_to_host_->bar_q_c_->arrive_and_wait();
        doc_to_host_->bar_a_h_->arrive_and_wait();
    }
};

class Doc_cmd : public Doc
{
public:
    Doc_cmd (const int &num, Shared_ptr<Data> &data, Shared_ptr<Doc_to_host> &doc_to_host) {
        num_ = num;
        data_ = data;
        prev_safe_ = -1;
        doc_to_host_ = doc_to_host;
    }

    void act(void) override {
        int target = -1;
        std::osyncstream(std::cout) << "Your choice:\n";
        std::cout.flush();

        while (true) {
            std::cin >> target; 
            
            if (data_->is_live_[target] && target != prev_safe_) {
                prev_safe_ = target;
                break;
            }
            else 
                std::osyncstream(std::cout) << "Wrong number, try again\n";
            std::cout.flush();
        }
        doc_to_host_->q_ = target;

        doc_to_host_->bar_q_c_->arrive_and_wait();
        doc_to_host_->bar_a_h_->arrive_and_wait();
    }

    void vote(void) override {
        vote_cmd(data_, num_);
    }
};

class Coma : public Player
{
public:
    std::queue<int> q_;
    std::set<int> s_;
    Shared_ptr<Coma_to_host> coma_to_host_;

    Coma () = default;

    Coma (const int &num, Shared_ptr<Data> &data, Shared_ptr<Coma_to_host> &coma_to_host) {
        num_ = num;
        data_ = data;
        coma_to_host_ = coma_to_host;
    }

    void state(void) {
        std::queue<int> new_q;

        while (!q_.empty()) {
            int vr = q_.front();
            q_.pop();

            if (data_->is_live_[vr])
                new_q.push(vr);
        }

        q_ = std::move(new_q);
    }

    void vote(void) override {
        state();
        int target = -1;

        if (q_.empty()) {
            while (true) {
                target = std::experimental::randint(0, int(data_->N_-1));
                
                if (data_->is_live_[target])
                    break;
            }
        } else {
            target = q_.front();
        }

        std::lock_guard<std::mutex> lg{*data_->mut_vote_};
        data_->vote_list_[num_] = target;
    }

    void act(void) override {
        state();
        int random_number = std::experimental::randint(0, int(1));
        int target = -1;

        if (!random_number % 2) { //rn = 0, kill;  rn = 1 question
            coma_to_host_->type_q_ = 0;

            if (q_.empty()) {
                while (true) {
                    target = std::experimental::randint(0, int(data_->N_-1));
                    
                    if (data_->is_live_[target] && target != num_)
                        break;
                }
            } else {
                target = q_.front();
                q_.pop();
            }
            
            coma_to_host_->q_ = target;

            coma_to_host_->bar_q_c_->arrive_and_wait();
            coma_to_host_->bar_a_h_->arrive_and_wait();
        } else {
            coma_to_host_->type_q_ = 1;

            while (true) {
                target = std::experimental::randint(0, int(data_->N_-1));
                
                if (data_->is_live_[target] && !s_.count(target) && target != num_)
                    break;
            }

            coma_to_host_->q_ = target;
            s_.insert(target);

            coma_to_host_->bar_q_c_->arrive_and_wait();
            coma_to_host_->bar_a_h_->arrive_and_wait();

            if (coma_to_host_->ans_)
                q_.push(target);
        }
    }
};

class Coma_cmd : public Coma
{
public:
    Coma_cmd (const int &num, Shared_ptr<Data> &data, Shared_ptr<Coma_to_host> &coma_to_host) {
        num_ = num;
        data_ = data;
        coma_to_host_ = coma_to_host;
    }

    void vote(void) override {
        vote_cmd(data_, num_);
    }

    void act(void) override {
        int number = 0;
        int target = -1;
        std::osyncstream(std::cout) << "Your choice(kill\\n 0 or check\\n 0):\n";
        std::cout.flush();

        std::string vr = "";

        do {
            std::cin >> vr;

            if (vr != "kill" and vr != "check") {
                std::osyncstream(std::cout) << "Wrong input, try again\n";
                std::cout.flush();
            }
        } while (vr != "kill" and vr != "check");

        number = vr == "check";

        while (true) {
            std::cin >> target; 
            
            if (data_->is_live_[target] && target != num_)
                break;
            else 
                std::osyncstream(std::cout) << "Wrong number, try again\n";
            std::cout.flush();
        }

        if (!number % 2) { //rn = 0, kill;  rn = 1 question
            coma_to_host_->type_q_ = 0;
            
            coma_to_host_->q_ = target;

            coma_to_host_->bar_q_c_->arrive_and_wait();
            coma_to_host_->bar_a_h_->arrive_and_wait();
        } else {
            coma_to_host_->type_q_ = 1;

            coma_to_host_->q_ = target;

            coma_to_host_->bar_q_c_->arrive_and_wait();
            coma_to_host_->bar_a_h_->arrive_and_wait();

            if (coma_to_host_->ans_) {
                std::osyncstream(std::cout) << target << " is Mafia\n";
            } else {
                std::osyncstream(std::cout) << target << " is Civillian\n";
            }

            std::cout.flush();
        }
    }
};

class Mana : public Player
{
public:
    Shared_ptr<Mana_to_host> mana_to_host_;

    Mana () = default;

    Mana (const int &num, Shared_ptr<Data> &data, Shared_ptr<Mana_to_host> &mana_to_host) {
        num_ = num;
        data_ = data;
        mana_to_host_ = mana_to_host;
    }

    void act(void) override {
        int target = -1;

        while (true) {
            target = std::experimental::randint(0, int(data_->N_-1));

            if (data_->is_live_[target] && target != num_)
                break;
        }

        mana_to_host_->q_ = target;

        mana_to_host_->bar_q_c_->arrive_and_wait();
        mana_to_host_->bar_a_h_->arrive_and_wait();
    } 
};

class Mana_cmd : public Mana 
{
public:
    Mana_cmd (const int &num, Shared_ptr<Data> &data, Shared_ptr<Mana_to_host> &mana_to_host) {
        num_ = num;
        data_ = data;
        mana_to_host_ = mana_to_host;
    }

    void vote(void) override {
        vote_cmd(data_, num_);
    }

    void act(void) override {
        int target = -1;
        std::osyncstream(std::cout) << "Your choice:\n";
        std::cout.flush();

        while (true) {
            std::cin >> target; 
            
            if (data_->is_live_[target] && target != num_)
                break;
            else 
                std::osyncstream(std::cout) << "Wrong number, try again\n";
            std::cout.flush();
        }

        mana_to_host_->q_ = target;

        mana_to_host_->bar_q_c_->arrive_and_wait();
        mana_to_host_->bar_a_h_->arrive_and_wait();
    } 
};

class Mafia : public Player 
{
public:
    Shared_ptr<Mafia_privat> maf_priv_;
    std::set<int> maf_bro_;

    Mafia () = default;

    Mafia (const int &num, Shared_ptr<Data> &data, Shared_ptr<Mafia_privat> & maf_priv, std::set<int> &maf_bro) {
        num_ = num;
        data_ = data;
        maf_priv_ = maf_priv;
        maf_bro_.insert(maf_bro.begin(), maf_bro.end());
    }

    void act(void) override {
        int target = -1;

        while (true) {
            target = std::experimental::randint(0, int(data_->N_-1));
            
            if (data_->is_live_[target] && !maf_bro_.count(target))
                break;
        }

        std::unique_lock<std::mutex> ul{*maf_priv_->mut_tar_};
        maf_priv_->s_target_.insert(target);
        maf_priv_->target_.insert(target);
        ul.unlock();

        maf_priv_->bar_maf_vote_->arrive_and_wait();

        maf_priv_->mafia_choice();
        
        maf_priv_->bar_maf_host_->arrive_and_wait();
    }

    void act_after_die(void) override {
        maf_priv_->bar_maf_vote_->arrive_and_wait();
        maf_priv_->mafia_choice();
        maf_priv_->bar_maf_host_->arrive_and_wait();
    }
};

class Mafia_cmd : public Mafia 
{
public:
    Mafia_cmd (const int &num, Shared_ptr<Data> &data, Shared_ptr<Mafia_privat> & maf_priv, std::set<int> &maf_bro) {
        num_ = num;
        data_ = data;
        maf_priv_ = maf_priv;
        maf_bro_.insert(maf_bro.begin(), maf_bro.end());
    }

    void vote(void) override {
        vote_cmd(data_, num_);
    }

    void act(void) override {
        int target = -1;

        maf_priv_->bar_maf_vote_->arrive_and_wait();

        std::unique_lock<std::mutex> ul{*maf_priv_->mut_tar_};
        std::osyncstream(std::cout) << "Maf bro choice:\n";

        for (auto i : maf_priv_->s_target_) {
            std::osyncstream(std::cout) << i << ": " << maf_priv_->target_.count(i) << "Maf bro\n";
        }
        std::osyncstream(std::cout) << "Your choice:\n";
        std::cout.flush();

        while (true) {
            std::cin >> target; 
            
            if (data_->is_live_[target] && !maf_bro_.count(target))
                break;
            else 
                std::osyncstream(std::cout) << "Wrong number, try again\n";
            std::cout.flush();
        }
        maf_priv_->s_target_.insert(target);
        maf_priv_->target_.insert(target);
        maf_priv_->tar_ = -1;
        ul.unlock();

        maf_priv_->mafia_choice();
        
        maf_priv_->bar_maf_host_->arrive_and_wait();
    }
};

