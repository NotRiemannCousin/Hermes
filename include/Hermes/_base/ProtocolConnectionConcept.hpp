#pragma once
#include <string_view>
#include <optional>
#include <string>
#include <format>


namespace Hermes {
    using std::same_as;

    /// @brief A ProtocolConnectionInfoConcept is an abstraction used to define ways to connect with sockets using a string.
    ///
    /// It allows parsing a raw connection string into a structured endpoint and converting it back to a string.
    ///
    /// Example with various connection types:
    /// @code{.cpp}
    /// /* TCP/IP & UDP
    ///  * "127.0.0.1:80"
    ///  * "192.168.1.100:443"
    ///  * "[2001:db8::1]:8080"
    ///  * "example.com:22"
    ///  */
    ///
    /// auto _ = Url::Parse("https://example.com:22");
    ///
    /// /* Unix Domain Sockets
    ///  * "/tmp/mysocket"
    ///  * "/var/run/docker.sock"
    ///  */
    ///
    /// auto _ = UnixDomainConnection::Parse("/var/run/docker.sock");
    ///
    /// /* Windows Named Pipes
    ///  * "\\\\.\\pipe\\mypipe"
    ///  * "\\\\ServerName\\pipe\\RemotePipe"
    ///  */
    ///
    /// auto _ = WindowsNamedPipe::Parse("\\\\ServerName\\pipe\\RemotePipe");
    ///
    /// /* Custom URI schemes
    ///  * "mqtt://broker.local:1883"
    ///  * "redis://127.0.0.1:6379"
    ///  * "postgresql://user:pass@localhost:5432/dbname"
    ///  */
    ///
    /// auto _ = PostgreSqlConnection::Parse("postgresql://user:pass@localhost:5432/dbname");
    ///
    /// /* Custom Non-URI schemes
    ///  * "WALKIE-TALKIE-TOY|=|network_1#user_23_RSA_key~"
    ///  * some crazy non-TCP/IP-UDP thing, sla, idk
    ///  */
    ///
    /// auto _ = WalkieTalkieConnection::Parse("walkie-talkie:network_1#user_23_RSA_key#crazy_thing_i_dont_know");
    ///
    /// @endcode
    ///
    /// You'll never use most of these cases (neither will I), but whatever.
    template<typename T>
    concept ProtocolConnectionInfoConcept =
        requires (T locator, std::string_view rawConnectionStr)
    {
        { T::Parse(rawConnectionStr) } -> same_as<std::optional<T>>;
        { std::format("{}", locator) } -> same_as<std::string>;
    };
} // namespace Hermes
