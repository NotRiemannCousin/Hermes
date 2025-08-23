# Hermes

## About

Hermes is a C++ Socket Wrapper library. It provides a simple, easy-to-use, and secure interface for
creating and using sockets. The goal of this lib is to use modern C++ features such as `std::expected`,
`std::ranges`, `std::generator`, and modules. Given that, the target version is C++26.

This lib implements only transport layer protocols, other libraries have to implement application layer protocols).
Right now, Hermes it's built with windows specific headers and there is no plans to support other operating systems
soon.

## Features

Implemented protocols:

- TCP
- UDP: in the future