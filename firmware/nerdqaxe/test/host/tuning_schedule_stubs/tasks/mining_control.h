#pragma once
namespace FiveTratumMining {
extern bool supported,paused,writesAllowed;
inline bool controlSupported(){return supported;}
inline bool miningWritesAllowed(){return writesAllowed;}
}
