/*
** Author : Mathieu CHASSARA
*/

#pragma once

#include    <Engine/Core/BaseScript.hpp>
#include    <Engine/Core/ScriptFactory.hpp>
#include    <Game/Scripts/ProgressBar.hpp>

class       WaveManager final : public BaseScript
{
public:
    WaveManager() = default;
    ~WaveManager() override final = default;

public:
    void    start() override final;
    void    update(float dt) override final;

    int     getWaves() const;
    void    setWaves(int waves);

private:
    void    startWave(uint32_t wave);
    bool    checkEndWave();

    void    handleStartWave();
    void    handleEndWave();

private:
    int     _waves;
    float   _timeBeforeWaveStarts;
    int _currentWave{-1};

    ProgressBar _progressBar;
};

REGISTER_SCRIPT(WaveManager);
