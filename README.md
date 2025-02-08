# Cryptolyser_Doppelganger

## The Cryptolyser project:
Cryptolyser is a project implementing Bernstein's cache timing attack on modern hardware.

* [Cryptolyser_Victim](https://github.com/MihaiMocanuGit/Cryptolyser_Victim): The server towards which the attack is directed.

* [Cryptolyser_Doppelganger](https://github.com/MihaiMocanuGit/Cryptolyser_Doppelganger): A near-clone of the victim server, used to study the timing data for known keys.

* [Cryptolyser_Attacker](https://github.com/MihaiMocanuGit/Cryptolyser_Attacker): The client that is attacking the server, using the timing data processed by the doppelganger.

* [Cryptolyser_Common](https://github.com/MihaiMocanuGit/Cryptolyser_Common): A submodule containing utility headers that are used by all the programs.
