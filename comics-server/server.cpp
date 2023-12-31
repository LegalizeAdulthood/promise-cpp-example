#include <boost/asio.hpp>
#include <boost/beast.hpp>

#include <comicsdb.h>
#include <json.h>

#include <add_ons/asio/io.hpp>
#include <promise-cpp/promise.hpp>

#include <iostream>
#include <regex>

namespace Comics = comicsdb::v2;
namespace asio = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
namespace ip = asio::ip;

using error_code = boost::system::error_code;
using socket_base = asio::socket_base;
using tcp = ip::tcp;

template <class Body, class Allocator>
using Request = http::request<Body, http::basic_fields<Allocator>>;
using Response = http::response<http::string_body>;

namespace comicsServer
{

struct Session
{
    explicit Session(tcp::socket &socket, Comics::ComicDb &db) :
        m_socket(std::move(socket)),
        m_db(db)
    {
    }

    ~Session()
    {
    }

    bool                             m_close{};
    tcp::socket                      m_socket;
    beast::flat_buffer               m_buffer;
    Comics::ComicDb                 &m_db;
    http::request<http::string_body> m_req;
};

// Promisified functions
static promise::Promise asyncAccept(tcp::acceptor &acceptor)
{
    return promise::newPromise(
        [&](promise::Defer &defer)
        {
            // Look up the domain name
            auto socket =
                std::make_shared<tcp::socket>(static_cast<asio::io_context &>(acceptor.get_executor().context()));
            acceptor.async_accept(*socket, [=](error_code err) { setPromise(defer, err, "resolve", socket); });
        });
}

// Returns a bad request response
Response badRequest(std::shared_ptr<Session> session, beast::string_view why)
{
    http::response<http::string_body> res{http::status::bad_request, session->m_req.version()};
    res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
    res.set(http::field::content_type, "text/html");
    res.keep_alive(session->m_req.keep_alive());
    res.body() = why;
    res.prepare_payload();
    return res;
};

// Returns a not found response
Response notFound(std::shared_ptr<Session> session)
{
    http::response<http::string_body> res{http::status::not_found, session->m_req.version()};
    res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
    res.set(http::field::content_type, "text/html");
    res.keep_alive(session->m_req.keep_alive());
    res.body() = "The resource '" + std::string{session->m_req.target()} + "' was not found.";
    res.prepare_payload();
    return res;
};

// Returns a server error response
Response serverError(std::shared_ptr<Session> session, beast::string_view what)
{
    http::response<http::string_body> res{http::status::internal_server_error, session->m_req.version()};
    res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
    res.set(http::field::content_type, "text/html");
    res.keep_alive(session->m_req.keep_alive());
    res.body() = "An error occurred: '" + std::string{what} + "'";
    res.prepare_payload();
    return res;
};

Response deleteComicResponse(std::shared_ptr<Session> session, int id)
{
    std::cout << "Delete comic " << id << '\n';
    deleteComic(session->m_db, id);
    Response res{http::status::ok, session->m_req.version()};
    res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
    res.set(http::field::content_type, "text/plain");
    res.keep_alive(session->m_req.keep_alive());
    res.body() = "Comic " + std::to_string(id) + " deleted.";
    res.prepare_payload();
    return res;
}

Response readComicResponse(std::shared_ptr<Session> session, int id)
{
    std::cout << "Read comic " << id << '\n';
    Comics::Comic comic = Comics::readComic(session->m_db, id);
    Response      res{http::status::ok, session->m_req.version()};
    res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
    res.set(http::field::content_type, "application/json");
    res.keep_alive(session->m_req.keep_alive());
    res.body() = toJson(comic);
    res.prepare_payload();
    return res;
}

Response createComicResponse(std::shared_ptr<Session> session)
{
    const std::string json = session->m_req.body();
    std::cout << "Create comic: " << json << '\n';
    Comics::Comic comic = Comics::fromJson(json);
    Response      res{http::status::ok, session->m_req.version()};
    res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
    res.set(http::field::content_type, "application/json");
    res.keep_alive(session->m_req.keep_alive());
    res.body() = toJson(comic);
    res.prepare_payload();
    return res;
}

Response updateComicResponse(std::shared_ptr<Session> session, int id)
{
    const std::string json = session->m_req.body();
    std::cout << "Update comic " << id << " to " << json << '\n';
    Comics::Comic comic = Comics::fromJson(json);
    updateComic(session->m_db, id, comic);
    Response res{http::status::ok, session->m_req.version()};
    res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
    res.set(http::field::content_type, "application/json");
    res.keep_alive(session->m_req.keep_alive());
    res.body() = toJson(comic);
    res.prepare_payload();
    return res;
}

// This function produces an HTTP response for the given
// request. The type of the response object depends on the
// contents of the request, so the interface requires the
// caller to pass a generic lambda for receiving the response.
template <class Send>
promise::Promise handleRequest(std::shared_ptr<Session> session, Send &&send)
{
    // Make sure we can handle the method
    bool             requiresId{};
    const http::verb method = session->m_req.method();
    switch (method)
    {
    case http::verb::delete_:
    case http::verb::get:
    case http::verb::put:
        requiresId = true;
        break;
    case http::verb::post:
        break;

    default:
        return send(badRequest(session, "Unknown HTTP-method"));
    }

    int id{-1};
    if (requiresId)
    {
        static const std::regex matchesId("^/comic/([0-9]+)$");
        const std::string       path{session->m_req.target()};
        std::smatch             parts;
        if (!std::regex_match(path, parts, matchesId))
        {
            return send(badRequest(session, "Malformed URI"));
        }
        id = std::stoi(parts[1]);
    }

    Response res;
    try
    {
        switch (method)
        {
        case http::verb::delete_:
            res = deleteComicResponse(session, id);
            break;

        case http::verb::get:
            res = readComicResponse(session, id);
            break;

        case http::verb::put:
            if (id == -1)
                res = createComicResponse(session);
            else
                res = updateComicResponse(session, id);
            break;

        case http::verb::post:
            res = createComicResponse(session);
            break;

        default:
            break;
        }
    }
    catch (const std::runtime_error &bang)
    {
        return send(notFound(session));
    }
    catch (...)
    {
        return send(serverError(session, "Internal error"));
    }

    return send(std::move(res));
}

// Report a failure
static int fail(error_code ec, char const *what)
{
    std::cerr << what << ": " << ec.message() << "\n";
    return EXIT_FAILURE;
}

// This is the C++11 equivalent of a generic lambda.
// The function object is used to send an HTTP message.
template <class Stream>
struct SendLambda
{
    explicit SendLambda(Stream &stream, bool &close) :
        m_stream(stream),
        m_close(close)
    {
    }

    template <bool isRequest, class Body, class Fields>
    promise::Promise operator()(http::message<isRequest, Body, Fields> &&msg) const
    {
        // Determine if we should close the connection after
        m_close = msg.need_eof();
        // The lifetime of the message has to extend
        // for the duration of the async operation so
        // we use a shared_ptr to manage it.
        auto sp = std::make_shared<http::message<isRequest, Body, Fields>>(std::move(msg));
        return promise::async_write(m_stream, *sp)
            .finally(
                [sp]()
                {
                    // sp will not be deleted util finally() called
                });
    }

private:
    Stream &m_stream;
    bool   &m_close;
};

// Handles an HTTP server connection
void handleSession(std::shared_ptr<Session> session)
{
    promise::doWhile(
        [=](promise::DeferLoop &loop)
        {
            //<1> Read a request
            session->m_req = {};
            promise::async_read(session->m_socket, session->m_buffer, session->m_req)
                .then(
                    [=]()
                    {
                        //<2> Send the response
                        // This lambda is used to send messages
                        SendLambda<tcp::socket> lambda{session->m_socket, session->m_close};
                        return handleRequest(session, lambda);
                    })
                .then(
                    []()
                    {
                        //<3> success, return default error_code
                        return boost::system::error_code();
                    },
                    [](const boost::system::error_code err)
                    {
                        //<3> failed, return the error_code
                        return err;
                    })
                .then(
                    [=](boost::system::error_code &err)
                    {
                        //<4> Keep-alive or close the connection.
                        if (!err && !session->m_close)
                        {
                            loop.doContinue(); // continue doWhile ...
                        }
                        else
                        {
                            session->m_socket.shutdown(tcp::socket::shutdown_send, err);
                            loop.doBreak(); // break from doWhile
                        }
                    });
        });
}

// Accepts incoming connections and launches the sessions
static int listenForConnections(asio::io_context &ioc, tcp::endpoint endpoint, Comics::ComicDb &db)
{
    error_code ec;

    // Open the acceptor
    auto acceptor = std::make_shared<tcp::acceptor>(ioc);
    acceptor->open(endpoint.protocol(), ec);
    if (ec)
        return fail(ec, "open");

    // Allow address reuse
    acceptor->set_option(socket_base::reuse_address(true));
    if (ec)
        return fail(ec, "set_option");

    // Bind to the server address
    acceptor->bind(endpoint, ec);
    if (ec)
        return fail(ec, "bind");

    // Start listening for connections
    acceptor->listen(socket_base::max_listen_connections, ec);
    if (ec)
        return fail(ec, "listen");

    std::cout << "Listening for connections on " << endpoint << '\n';

    promise::doWhile(
        [acceptor, &db](promise::DeferLoop &loop)
        {
            asyncAccept(*acceptor)
                .then(
                    [&](std::shared_ptr<tcp::socket> socket)
                    {
                        auto session = std::make_shared<Session>(*socket, db);
                        handleSession(session);
                    })
                .fail([](const error_code err) {})
                .then(loop);
        });

    ioc.run();

    return EXIT_SUCCESS;
}

static int run(int argc, char *argv[])
{
    // Check command line arguments.
    if (argc != 4)
    {
        std::cerr << "Usage: " << argv[0] << " <address> <port> <threads>\n"
                  << "Example:\n"
                  << "    " << argv[0] << " 0.0.0.0 8080 1\n";
        return EXIT_FAILURE;
    }
    auto const address = ip::make_address(argv[1]);
    auto const port = static_cast<unsigned short>(std::atoi(argv[2]));
    auto const threads = std::max<int>(1, std::atoi(argv[3]));

    asio::io_context ioc{threads};

    Comics::ComicDb db = Comics::load();

    return listenForConnections(ioc, tcp::endpoint{address, port}, db);
}

} // namespace comicsServer

int main(int argc, char *argv[])
{
    return comicsServer::run(argc, argv);
}
