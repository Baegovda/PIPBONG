#pragma once
#include <QPoint>
#include <QString>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

// Owner-thread arbitration only. The backend is the sole physical cursor lock.
class MouseLockClaims {
    struct State;
public:
    using Token=quint64;
    struct Owner {
        std::weak_ptr<void> controller,context;
        QString profile,feature;
        std::pair<std::wstring,std::wstring> binding;
        quint64 run=0;
        bool live()const;
        bool same(const Owner&)const;
    };
    enum class Kind { Feature,Early };
    struct Backend {std::function<void(QPoint)> apply;std::function<void()> release;};
    struct Winner {Token token=0;Owner owner;Kind kind=Kind::Feature;QPoint point;};
    using Provider=std::function<std::optional<QPoint>()>;
    class Batch {
        friend class MouseLockClaims;
        explicit Batch(std::shared_ptr<State>);
        std::shared_ptr<State> m_state;
    public:
        Batch(const Batch&)=delete;Batch& operator=(const Batch&)=delete;
        Batch(Batch&&)=default;~Batch();
    };
    explicit MouseLockClaims(Backend);
    ~MouseLockClaims();
    MouseLockClaims(const MouseLockClaims&)=delete;
    MouseLockClaims& operator=(const MouseLockClaims&)=delete;
    Batch batch();
    Token upsert(Token,const Owner&,Kind,Provider);
    void release(Token);
    void retain(const std::weak_ptr<void>& controller,const std::vector<Owner>& registered);
    void releaseController(const std::weak_ptr<void>&);
    void clear();
    void refresh();
    bool hasClaims(const std::weak_ptr<void>& controller)const;
    std::optional<Winner> winner()const;
private:
    static void sync(const std::shared_ptr<State>&);
    std::shared_ptr<State> m_state;
};
