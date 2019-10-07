#include <iostream>
#include <thread>
#include <random>
#include <chrono>
#include <functional>
#include <array>
#include "messageQueue.hpp"


using system_clock = std::chrono::system_clock;
using duration = std::chrono::duration<double>;


enum class Action {
    ACTION_NONE,
    ACTION_1,
    ACTION_2,
    ACTION_3,
    ACTION_4,
    ACTION_5,
    ACTION_6,
    ACTION_7,

};

std::ostream& operator<<(std::ostream& os, Action const& action) {
    os << static_cast<int>(action);
    return os;
}

class ListenerOne: public mq::Listener<Action> {
    std::array<Action, 3> actions {
        Action::ACTION_1,
        Action::ACTION_2,
        Action::ACTION_3,
    };
    virtual bool consumed(Action const& message) const noexcept override {
        for (auto&& a : actions) {
            if (message == a) return true;
        }
        return false;
    }
};

class ListenerTwo: public mq::Listener<Action> {
    std::array<Action, 4> actions {
        Action::ACTION_4,
        Action::ACTION_5,
        Action::ACTION_6,
        Action::ACTION_7,
    };
    virtual bool consumed(Action const& message) const noexcept override {
        for (auto&& a : actions) {
            if (message == a) return true;
        }
        return false;
    }
};


class ListenerTaskOne {
    std::mt19937 gen{std::random_device{}()};
    std::uniform_int_distribution<> dis{2, 4};

    void process(Action const& message) {
        std::cout << "ListenerOne consume action: ";
        switch (message) {
        case Action::ACTION_1:
            std::cout << "ACTION_1\n";
            break;
        case Action::ACTION_2:
            std::cout << "ACTION_2\n";
            break;
        case Action::ACTION_3:
            std::cout << "ACTION_3\n";
            break;
        default:
            std::cout << "None\n";
            break;
        }
    }

public:
    ListenerOne listener{};
    void operator()() {
        while (true) {
            Action message{Action::ACTION_NONE};
            try {
                message = listener.listen();
            } catch (mq::BaseMessageQueueException const& e) {
                std::cout << e.what() << "\n";
            }
            process(message);
            // Simulate some time-consuming task.
            std::this_thread::sleep_for(duration(dis(gen)));
        }
    }
};

class ListenerTaskTwo {
    std::mt19937 gen{std::random_device{}()};
    std::uniform_int_distribution<> dis{1, 6};

    void process(Action const& message) {
        std::cout << "ListenerTwo consume action: ";
        switch (message) {
        case Action::ACTION_4:
            std::cout << "ACTION_4\n";
            break;
        case Action::ACTION_5:
            std::cout << "ACTION_4\n";
            break;
        case Action::ACTION_6:
            std::cout << "ACTION_6\n";
            break;
        case Action::ACTION_7:
            std::cout << "ACTION_7\n";
            break;
        default:
            std::cout << "None\n";
            break;
        }
    }

public:
    ListenerTwo listener{};
    void operator()() {
        while (true) {
            Action message{Action::ACTION_NONE};
            try{
                message = listener.listen();
            } catch (mq::BaseMessageQueueException const& e) {}
            process(message);
            // Simulate some time-consuming task.
            std::this_thread::sleep_for(duration(dis(gen)));
        }
    }
};


class ProducerTask {
    std::array<Action, 7> actions{
        Action::ACTION_1,
        Action::ACTION_2,
        Action::ACTION_3,
        Action::ACTION_4,
        Action::ACTION_5,
        Action::ACTION_6,
        Action::ACTION_7,
    };
    std::mt19937 gen{std::random_device{}()};
    std::uniform_int_distribution<> dis{10, 20};
    std::uniform_int_distribution<> enum_dis{
        0, static_cast<int>(actions.size()) - 1
    };


public:
    mq::Producer<Action> producer{};
    void operator()() {
        while (true) {
            std::cout << "ProducerTask Produce\n";
            producer.send(actions[enum_dis(gen)]);
            std::this_thread::sleep_for(duration(dis(gen)));
        }
    }
};

int main() {

    ProducerTask producer_task{};
    ListenerTaskOne listener_task{};
    ListenerTaskTwo listener_task_two{};
    producer_task.producer.attach(listener_task.listener);
    producer_task.producer.attach(listener_task_two.listener);
    producer_task.producer.set_max_len(10);
    listener_task.listener.set_blocking(true, 30);

    // The arguments of the std:thread ctor are moved or copied by value.
    std::thread producer_thread{std::ref(producer_task)};
    std::thread listener_thread{std::ref(listener_task)};
    std::thread listener_two_thread{std::ref(listener_task_two)};
    listener_thread.join();
    listener_two_thread.join();
    producer_thread.join();
    return 0;
}
