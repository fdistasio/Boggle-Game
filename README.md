# Boggle
The project concerns the game Boggle (multiplayer). This consists of
compose words using the letters positioned on a square matrix of 16 letters (4x4). Each word must consist of at least 4 characters and can be composed using the letters of the matrix only once. The boxes used must be contiguous and composed horizontally or vertically but not diagonally, in this version. It is not possible to move over a box more than once for the same word.

The project is implemented with multithreaded servers and clients and focuses on concurrent programming. Consequently, other aspects such as security or optimization of other mechanisms have not been explored in depth. For more information regarding the implementation read the [report](docs/Relazione.pdf)
