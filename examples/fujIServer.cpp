#include <Hermes/Socket/ListenerSocket.hpp>
#include <Hermes/Utils/UntilMatch.hpp>

#include <string_view>
#include <algorithm>
#include <ranges>
#include <print>
#include <format>
#include <expected>
#include <ostream>

namespace rg = std::ranges;
namespace vs = std::views;

std::expected<std::monostate, std::string> RunServer() {
    using namespace std::literals::string_view_literals;
    using Hermes::RawTcpListener;
    using Hermes::RawTcpServer;

    const Hermes::IpEndpoint endpoint{ Hermes::IpAddress::FromIpv4({127, 0, 0, 1}), 8080 };

    static constexpr auto s_log = [](std::string message) {
        return [message = std::move(message)]<class T>(T&& obj) -> T {
            std::print("{}", message);
            return std::forward<T>(obj);
        };
    };
    static constexpr auto s_sendRequest = [](RawTcpServer&& socket) -> Hermes::ConnectionResultOper {
        auto socketView{ socket.RecvLazyRange<char>() };

        const auto requestLine{ socketView | Hermes::Utils::UntilMatch("\r\n"sv) | rg::to<std::string>() };
        std::println("Request Line:\n{}", requestLine);

        const auto headers{ socketView | Hermes::Utils::UntilMatch("\r\n\r\n"sv) | rg::to<std::string>() };
        std::println("Headers:\n{}", headers);

        static int index{};
        constexpr std::array lines{
            R"(Parece bobagem / 馬鹿みたいだ / baka mitai da)",

            R"(Tuas palavras gentis / 君の優しい言葉 / kimi no yasashii kotoba)",

            R"(Desarrumam meu ritmo / 乱されるペース / midasareru peesu)",

            R"(Eu preciso virar adulto / 大人にならなくちゃね / otona ni naranakucha ne)",

            R"(Mesmo que seja impossível, não é? / なんて無理なのにね / nante muri na noni ne)",


            R"(Eu faria qualquer coisa por você / 君にならなんだってしてあげたい / kimi ni nara nan datte shite agetai)",

            R"(Se fosse contigo, eu fugiria junto / 君とならふたりで逃げたい / kimi to nara futari de nigetai)",

            R"(Porque é você, eu não consigo ficar são / 君だからまともじゃいられない / kimi dakara matomo ja irarenai)",

            R"(Quero estar mais perto de você / もっとそばにいきたい / motto soba ni ikitai)",


            R"(Te observando como se estivesse rezando / 祈るように君をただ見つめていた / inoru you ni kimi wo tada mitsumeteita)",

            R"(Correndo atrás das pontas dos teus dedos / 指先をチェイス / yubisaki wo cheisu)",

            R"(O que você está pensando agora? / 今どんなことを思っているの / ima donna koto wo omotteiru no)",

            R"(Me conta / 教えて / oshiete)",


            R"(Eu faria qualquer coisa por você / 君にならなんだってしてあげたい / kimi ni nara nan datte shite agetai)",

            R"(Só você consegue mexer com meu coração / 君だけが心揺さぶる / kimi dake ga kokoro yusaburu)",

            R"(Se eu não fosse eu mesmo / もし僕が僕じゃなかったら / moshi boku ga boku janakattara)",

            R"(Eu poderia ter estado sempre ao seu lado / ずっとそばにいれたの / zutto soba ni ireta no)",

            R"(Eu só preciso de você / 君しかいらない / kimi shika iranai)",


            R"(Noite sagrada / Holy night / Holy night)",

            R"(Sua noite solitária / Your lonely night / Your lonely night)",


            R"(Para sempre assim, ainda me sinto incompleto sem você / いつまでもこうして君が足りなくて / itsumademo koushite kimi ga tarinakute)",

            R"(Mesmo que eu tenha caminhado sozinho até agora / これまで孤独で歩いてきたのに / kore made kodoku de aruitekita noni)",

            R"(Você é diferente, completamente diferente de qualquer outra pessoa / 君だけは違って　誰とも違って / kimi dake wa chigatte dare tomo chigatte)",


            R"(Eu faria qualquer coisa por você / 君にならなんだってしてあげたい / kimi ni nara nan datte shite agetai)",

            R"(Se fosse contigo, eu fugiria junto / 君とならふたりで逃げたい / kimi to nara futari de nigetai)",

            R"(Porque é você, eu não consigo ficar são / 君だからまともじゃいられない / kimi dakara matomo ja irarenai)",

            R"(Quero estar mais perto de você / もっとそばにいきたいよ / motto soba ni ikitai yo)",


            R"(Eu faria qualquer coisa por você / 君にならなんだってしてあげたい / kimi ni nara nan datte shite agetai)",

            R"(Só você consegue mexer com meu coração / 君だけが心揺さぶる / kimi dake ga kokoro yusaburu)",

            R"(Se eu não fosse eu mesmo / もし僕が僕じゃなかったら / moshi boku ga boku janakattara)",

            R"(Eu poderia ter estado sempre ao seu lado / ずっとそばにいれたのに / zutto soba ni ireta noni)",


            R"(Eu queria estar sempre ao seu lado / ずっとそばにいたかった / zutto soba ni itakatta)",

            R"(Eu só preciso de você / 君しかいらない / kimi shika iranai)",
        };

        const auto body{ std::format(R"(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
</head>
<body>
  <h1>fujI</h1>
  <h2>{}</h2>
</body>
<style>
body {{
  font-size: xx-large;
  background-image: url("https://is1-ssl.mzstatic.com/image/thumb/Music116/v4/16/31/4d/16314d33-5eff-574d-496b-eb79ce616719/4547366554519.jpg/300x300bb.webp");
  background-repeat: no-repeat;
  background-attachment: fixed;
  background-size: cover;
  background-position: center;
  backdrop-filter: blur(10px);

  color: white;
  text-align: center;
  width: 100vw;
  height: 100vh;

  display: flex;
  flex-direction: column;
  justify-content: center;
  align-items: center
}}
body, html{{
  margin: 0;
  box-sizing: border-box;
}}
</style>
</html>
        )", lines[index % lines.size()]) };
        const auto response{
            std::format(
                "HTTP/1.1 200 OK\r\n"
                "Server: Hermes/0.2\r\n"
                "Content-Type: text/html\r\n"
                "Content-Length: {}\r\n"
                "Connection: close\r\n\r\n"
                "{}",
                body.size(), body) };

        index++;
        return socket.Send(response).second;
    };


    static constexpr auto s_loop = [](auto value) -> Hermes::ConnectionResultOper {
        for (auto&& sla : value.AcceptAll()) {
            if (!sla) return std::unexpected{ sla.error() };
            auto _{ s_sendRequest(std::move(*sla)) };
        }
        return std::monostate{};
    };

    return RawTcpListener::ListenOne(Hermes::DefaultSocketData<>{ endpoint })
            .and_then(s_loop)
            .transform_error([](auto err){ return std::format("{:v}", err); });
}

int main() {
    if (const auto res{ RunServer() }; !res) {
        int err{ WSAGetLastError() };
        
        std::println("\n\n{} : {}", res.error(), err);
    } else {
        std::println("\n\nServer finished successfully.");
    }

    return 0;
}