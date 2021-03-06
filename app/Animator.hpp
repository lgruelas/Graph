#pragma once

#include "Geometry/AllConvex.hpp"
#include "MessageBox.hpp"
#include "Utility.hpp"
#include <SFML/Graphics.hpp>
#include <functional>
#include <sstream>
#include <unordered_map>

class Scene
{
public:
    explicit Scene(real duration = 0.0) : m_duration(duration)
    {
        if (m_duration < 0.0001) // to avoid divisions by 0 and weird negative
                                 // number durations.
            m_duration = 0.0001;
    }
    virtual ~Scene() = default;

    void Update(real t)
    {
        m_time += t;
        ChildUpdate();
    }

    real ElapsedTime() const { return m_time; }

    real ElapsedPercentage() const { return m_time/m_duration; }

    real RemainingTime() const { return m_duration - m_time; }

    bool Finished() const { return m_time > m_duration; }

    void OnStart()
    {
        for (auto& s : start)
            s();
        m_time = 0;
    }

    void OnFinish()
    {
        for (auto& f : finish)
            f();
        m_time = 0;
    }

    template <class GUI_t>
    Scene* AddStartMessage(GUI_t& gui,
                           const std::string& msg,
                           const sf::Color& color = sf::Color::White,
                           real duration = 5.0)
    {
        start.emplace_back([&gui, msg, color, duration]() {
            gui.AddMessage(msg, color, duration);
        });
        return this;
    }

    template <class GUI_t>
    Scene* AddFinishMessage(GUI_t& gui,
                            const std::string& msg,
                            const sf::Color& color = sf::Color::White,
                            real duration = 5.0)
    {
        finish.emplace_back([&gui, msg, color, duration]() {
            gui.AddMessage(msg, color, duration);
        });
        return this;
    }

    template <class Func>
    Scene* AddStartAction(Func f)
    {
        start.emplace_back(f);
        return this;
    }

    template <class Func>
    Scene* AddFinishAction(Func f)
    {
        finish.emplace_back(f);
        return this;
    }

protected:
    virtual void ChildUpdate(){};

private:
    real m_time{0};
    real m_duration{5.0};

    std::vector<std::function<void(void)>> start{};
    std::vector<std::function<void(void)>> finish{};
};

template <class T>
class InterpolatorScene : public Scene
{
public:
    InterpolatorScene(T& val, const T& end, real duration)
        : Scene(duration)
        , m_val(&val)
        , m_start(val)
        , m_end(end)
        , set_start_on_start(true)
    {}

    InterpolatorScene(T& val, const T& start, const T& end, real duration)
        : Scene(duration), m_val(&val), m_start(start), m_end(end)
    {}

    void ChildUpdate() override
    {
        real t = ElapsedPercentage();

        if (set_start_on_start)
        {
            m_start = *(m_val);

            set_start_on_start = false;
            return;
        }

        *(m_val) = interpolate(t, m_start, m_end);
    }

private:
    T* m_val;
    T m_start;
    T m_end;
    bool set_start_on_start{false};
};

template <class Client>
class Animation
{
public:
    explicit Animation(Client* parent) : m_parent(parent)
    {
        scenes.reserve(100);
        current_scene = scenes.begin();
    }

    Animation(const Animation& A) = delete;
    Animation(Animation&& A) = delete;
    void operator=(const Animation& A) = delete;
    void operator=(Animation&& A) = delete;
    ~Animation() = default;

    void Reset()
    {
        current_scene = scenes.begin();

        current_scene_has_started = false;
    }

    void Update(real time)
    {
        if (current_scene == scenes.end())
        {
            if (InALoop())
                Reset();
            return;
        }

        if (Paused())
            return;

        if ((*current_scene)->Finished())
        {
            (*current_scene)->OnFinish();
            ++current_scene;
            current_scene_has_started = false;

            if (pause_after_scene)
                Pause();
            return;
        }

        if (!current_scene_has_started)
        {
            (*current_scene)->OnStart();
            current_scene_has_started = true;
        }

        (*current_scene)->Update(time);
    }

    template <class T, class U>
    Scene* AddScene(T& val, const U& end, real duration)
    {
        scenes.emplace_back(new InterpolatorScene<T>(val, end, duration));
        Reset();
        return scenes.back().get();
    }

    template <class T>
    Scene* AddScene(T& val, const T& start, const T& end, real duration)
    {
        scenes.emplace_back(
          new InterpolatorScene<T>(val, start, end, duration));
        Reset();
        return scenes.back().get();
    }

    template <class T>
    Scene* AddScene(T& val, const T& end, real duration)
    {
        scenes.emplace_back(new InterpolatorScene<T>(val, end, duration));
        Reset();
        return scenes.back().get();
    }

    template <class T, class U>
    Scene* AddScene(T& val, const U& start, const U& end, real duration)
    {
        scenes.emplace_back(
          new InterpolatorScene<T>(val, start, end, duration));
        Reset();
        return scenes.back().get();
    }

    template <class GUI_t>
    Scene*
    AddScene(GUI_t& gui, const std::string& message, const sf::Color& color)
    {
        scenes.emplace_back(new Scene(1.0));
        scenes.back()->AddStartMessage(gui, message, color);
        Reset();
        return scenes.back().get();
    }

    bool Paused() const { return paused == 0; }

    void Pause()
    {
        if (paused > 0 && !sf::Keyboard::isKeyPressed(sf::Keyboard::Space))
            --paused;
    }

    void Play()
    {
        if (paused < 2)
            ++paused;
    }

    void PauseAfterEveryScene(bool paes)
    {
        pause_after_scene = paes;
        if (paes)
            Pause();
    }

    void SetLoop(bool in_loop = true) { loop = in_loop; }

    bool InALoop() const { return loop; }

private:
    using scene_container = std::vector<std::unique_ptr<Scene>>;
    using scene_iterator = scene_container::iterator;
    scene_container scenes{};
    scene_iterator current_scene{};
    int paused{0};
    bool pause_after_scene{false};

    bool current_scene_has_started{false};

    bool loop{false};

    Client* m_parent{nullptr};
};
