#ifndef MESSAGE_QUEUE
#define MESSAGE_QUEUE

#include <concepts>
#include <memory>
#include <mutex>
#include <optional>
#include <type_traits>
#include <utility>

#include "synchronizer.hpp"

namespace mq {

enum class Mode {
    FIFO,
    LIFO,
};

template <typename Q>
concept ValidQueue = requires(Q q)
{
    typename Q::value_type;
    q.pop_front();
    q.pop_back();
    q.push_back(std::declval<typename Q::value_type>());
    {
        q.back()
    }
    ->std::same_as<typename Q::value_type&>;
    {
        q.back()
    }
    ->std::same_as<typename Q::value_type&>;
    {
        q.size()
    }
    ->std::convertible_to<std::size_t>;
    {
        q.empty()
    }
    ->std::convertible_to<bool>;
};

template <std::copy_constructible Mtype>
class BaseQueue {
public:
    virtual void pop_front() = 0;
    virtual void pop_back() = 0;
    virtual void push(Mtype const& msg) = 0;
    virtual Mtype& back() = 0;
    virtual Mtype& front() = 0;
    virtual std::size_t size() = 0;
    virtual bool empty() = 0;
    virtual ~BaseQueue() = default;
};

template <std::copy_constructible Mtype, ValidQueue QueueType>
class DerivedQueue : public BaseQueue<Mtype> {
public:
    explicit DerivedQueue(QueueType&& queue_)
        : queue { std::move(queue_) }
    {
    }

    void pop_front() final { queue.pop_front(); }
    void pop_back() final { queue.pop_back(); }
    void push(Mtype const& msg) final { queue.push_back(msg); }
    Mtype& back() final { return queue.back(); }
    Mtype& front() final { return queue.front(); }
    std::size_t size() final { return queue.size(); }
    bool empty() final { return queue.empty(); }

private:
    QueueType queue;
};

template <std::copy_constructible Mtype>
class BaseQueueManipulator {
public:
    virtual void pop(BaseQueue<Mtype>& messq) = 0;
    virtual Mtype const& get(BaseQueue<Mtype>& messq) const = 0;
    virtual void push(Mtype const& msg, BaseQueue<Mtype>& messq)
    {
        messq.push(msg);
    }
    virtual Mode get_mode() const noexcept { return qmode; }
    virtual ~BaseQueueManipulator() = default;
    explicit BaseQueueManipulator(Mode qmode_)
        : qmode { qmode_ }
    {
    }

private:
    Mode const qmode;
};

template <std::copy_constructible Mtype>
class QueueManipulatorFIFO : public BaseQueueManipulator<Mtype> {
public:
    QueueManipulatorFIFO()
        : BaseQueueManipulator<Mtype> { Mode::FIFO }
    {
    }

    void pop(BaseQueue<Mtype>& messq) final { messq.pop_front(); }
    Mtype const& get(BaseQueue<Mtype>& messq) const final
    {
        return messq.front();
    }
};

template <std::copy_constructible Mtype>
class QueueManipulatorLIFO : public BaseQueueManipulator<Mtype> {
public:
    QueueManipulatorLIFO()
        : BaseQueueManipulator<Mtype> { Mode::LIFO }
    {
    }

    void pop(BaseQueue<Mtype>& messq) final { messq.pop_back(); }
    Mtype const& get(BaseQueue<Mtype>& messq) const final { return messq.back(); }
};

template <std::copy_constructible Mtype>
class Queue {
public:
    template <ValidQueue QueueType>
    explicit Queue(QueueType&& msg_queue_, std::size_t max_size_ = 1000)
        : msg_queue { std::make_unique<DerivedQueue<Mtype, std::decay_t<QueueType>>>(
            std::move(msg_queue_)) }
        , max_size { max_size_ }
        , count_full { max_size_, 0 }
        , count_empty { max_size_, max_size_ }
    {
    }

    std::optional<MType> dequeue_if(std::predicate<MType const&> auto const& pred)
    {
        sync::Synchronizer s { count_full, count_empty, mutex };
        if (msg_queue->empty())
            return {};
        MType const& m = queue_manipulator->get(*msg_queue);
        if (pred(m)) {
            pop();
            return { m };
        }
        return {};
    }

    bool enqueue(Mtype const& msg)
    {
        sync::Synchronizer s { count_empty, count_full, mutex };
        return push(msg);
    }

    void set_mode(Mode new_mode)
    {
        std::lock_guard lck { mutex };
        switch (new_mode) {
        case Mode::FIFO:
            queue_manipulator.reset(new QueueManipulatorFIFO<Mtype> {});
            break;
        case Mode::LIFO:
            queue_manipulator.reset(new QueueManipulatorLIFO<Mtype> {});
            break;
        default:
            break;
        }
    }

    Mode mode() const
    {
        std::lock_guard lck { mutex };
        return queue_manipulator->get_mode();
    }

private:
    bool full() const { return msg_queue->size() == max_size; }
    bool empty() const { return msg_queue->empty(); }
    void pop() { queue_manipulator->pop(*msg_queue); }
    std::size_t size() const noexcept { return max_size; }
    // std::size_t count() const noexcept { return msg_queue->size(); }
    bool push(Mtype const& msg)
    {
        if (full())
            return false;
        queue_manipulator->push(msg, *msg_queue);
#ifdef DEBUG
        std::cout << "Queue size after push: " << msg_queue->size() << '\n';
#endif
        return true;
    }
    std::unique_ptr<BaseQueueManipulator<Mtype>> queue_manipulator {
        new QueueManipulatorLIFO<Mtype> {}
    };
    std::unique_ptr<BaseQueue<Mtype>> msg_queue;
    std::mutex mutex {};
    std::size_t const max_size;
    sem::Semaphore count_full, count_empty;
};

template <typename Mtype = void, ValidQueue QueueType>
explicit Queue(QueueType&&, std::size_t)
    -> Queue<typename std::remove_cvref_t<QueueType>::value_type>;

template <std::copy_constructible Mtype>
class Receiver {
public:
    explicit Receiver(Queue<Mtype>& q)
        : queue { q }
    {
    }

    std::optional<MType> dequeue_if(std::predicate<MType const&> auto&& pred)
    {
        return queue.dequeue_if(std::forward<decltype(pred)>(pred));
    }

private:
    Queue<Mtype>& queue;
};
template <std::copy_constructible Mtype>
Receiver(Queue<Mtype>&) -> Receiver<Mtype>;

template <std::copy_constructible Mtype>
class BlockingReceiver : public Receiver<Mtype> {
};

template <std::copy_constructible Mtype>
class Producer {
public:
    explicit Producer(Queue<Mtype>& q)
        : queue { q }
    {
    }
    bool enqueue(Mtype const& msg) { return queue.enqueue(msg); }

private:
    Queue<Mtype>& queue;
};
template <std::copy_constructible Mtype>
Producer(Queue<Mtype>&) -> Producer<Mtype>;
} // namespace mq

#endif
