#include <add_ons/asio/io.hpp>
#include <boost/beast.hpp>
#include <promise-cpp/promise.hpp>

#include <comicsdb.h>
#include <json.h>

#include <cstdlib>
#include <iostream>
#include <map>

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;

using tcp = asio::ip::tcp;

namespace Comics = comicsdb::v2;

namespace comicsClient
{

struct Session : std::enable_shared_from_this<Session>
{
    explicit Session(const std::string &server, asio::io_context &ioc, Comics::ComicDb &db) :
        m_server(server),
        m_resolver(ioc),
        m_socket(ioc),
        m_db(db)
    {
    }

    promise::Promise readRemoteComic(int id);
    promise::Promise updateRemoteComic(int localId);

    std::string                       m_server;
    tcp::resolver                     m_resolver;
    tcp::socket                       m_socket;
    beast::flat_buffer                m_buffer;
    http::request<http::string_body>  m_req;
    http::response<http::string_body> m_res;
    Comics::ComicDb                  &m_db;
    int                               m_id{-1};
    std::map<int, int>                m_remoteIds;
};

promise::Promise sendRequest(std::shared_ptr<Session> session, const std::string &host, const std::string &port)
{
    //<1> Resolve the host
    return promise::async_resolve(session->m_resolver, host, port)
        .then(
            [=](tcp::resolver::results_type &results)
            {
                //<2> Connect to the host
                return promise::async_connect(session->m_socket, results);
            })
        .then(
            [=]()
            {
                //<3> Write the request
                return promise::async_write(session->m_socket, session->m_req);
            })
        .then(
            [=](size_t bytes_transferred)
            {
                boost::ignore_unused(bytes_transferred);
                //<4> Read the response
                return promise::async_read(session->m_socket, session->m_buffer, session->m_res);
            })
        .then(
            [=](size_t bytes_transferred)
            {
                boost::ignore_unused(bytes_transferred);
                //<5> Response is in m_res
            })
        .then(
            []()
            {
                //<6> success, return default error_code
                return boost::system::error_code();
            },
            [](const boost::system::error_code err)
            {
                //<6> failed, return the error_code
                return err;
            })
        .then(
            [=](boost::system::error_code &err)
            {
                //<7> Gracefully close the socket
                session->m_socket.shutdown(tcp::socket::shutdown_both, err);
            });
}

promise::Promise getRequest(std::shared_ptr<Session> session, std::string const &host, std::string const &port,
                            std::string const &target, int version)
{
    // Set up an HTTP GET request message
    session->m_req = {};
    session->m_req.version(version);
    session->m_req.method(http::verb::get);
    session->m_req.target(target);
    session->m_req.set(http::field::host, host);
    session->m_req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
    return sendRequest(session, host, port);
}

promise::Promise Session::readRemoteComic(int id)
{
    return getRequest(shared_from_this(), m_server, "8000", "/comic/" + std::to_string(id), 10);
}

promise::Promise putRequest(std::shared_ptr<Session> session, std::string const &host, std::string const &port,
                            std::string const &target, int version, int localId)
{
    // Set up an HTTP PUT request message
    session->m_req = {};
    session->m_req.version(version);
    session->m_req.method(http::verb::put);
    session->m_req.target(target);
    session->m_req.set(http::field::host, host);
    session->m_req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
    session->m_req.set(http::field::content_type, "application/json");
    session->m_req.body() = toJson(session->m_db[localId]);
    session->m_req.prepare_payload();
    return sendRequest(session, host, port);
}

promise::Promise Session::updateRemoteComic(int localId)
{
    int remoteId = m_remoteIds[localId];
    return putRequest(shared_from_this(), m_server, "8000", "/comic/" + std::to_string(remoteId), 10, localId);
}

static int run(int argc, char *argv[])
{
    if (argc != 2)
    {
        std::cerr << "Usage:\n    " << argv[0] << " <server>\n";
        return EXIT_FAILURE;
    }

    const std::string server{argv[1]};
    asio::io_context  ioc;
    Comics::ComicDb   db;
    auto              session = std::make_shared<Session>(server, ioc, db);

    session->readRemoteComic(0)
        .then(
            [=]
            {
                session->m_id = createComic(session->m_db, Comics::fromJson(session->m_res.body()));
                session->m_remoteIds.emplace(session->m_id, 0);
                Comics::Comic comic = readComic(session->m_db, session->m_id);
                comic.pencils = Comics::findPerson("Steve Ditko");
                updateComic(session->m_db, session->m_id, comic);
            })
        .then(
            [=]
            {
                std::cout << "Updated local comic: " << toJson(session->m_db[session->m_id]) << '\n';
                return session->updateRemoteComic(session->m_id);
            })
        .then([] { std::cout << "Remote comic updated\n"; });

    ioc.run();

    return EXIT_SUCCESS;
}

} // namespace comicsClient

int main(int argc, char *argv[])
{
    return comicsClient::run(argc, argv);
}
