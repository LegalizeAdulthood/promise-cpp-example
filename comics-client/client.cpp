#include <add_ons/asio/io.hpp>
#include <boost/beast.hpp>
#include <promise-cpp/promise.hpp>

#include <comicsdb.h>
#include <json.h>

#include <cstdlib>
#include <iostream>

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
        resolver_(ioc),
        socket_(ioc),
        m_db(db)
    {
    }

    promise::Promise readComic(int id);
    promise::Promise updateComic(int id);

    std::string                       m_server;
    tcp::resolver                     resolver_;
    tcp::socket                       socket_;
    beast::flat_buffer                buffer_;
    http::request<http::string_body>  req_;
    http::response<http::string_body> res_;
    Comics::ComicDb                  &m_db;
};

promise::Promise sendRequest(std::shared_ptr<Session> session, const std::string &host, const std::string &port)
{
    //<1> Resolve the host
    return promise::async_resolve(session->resolver_, host, port)
        .then(
            [=](tcp::resolver::results_type &results)
            {
                //<2> Connect to the host
                return promise::async_connect(session->socket_, results);
            })
        .then(
            [=]()
            {
                //<3> Write the request
                return promise::async_write(session->socket_, session->req_);
            })
        .then(
            [=](size_t bytes_transferred)
            {
                boost::ignore_unused(bytes_transferred);
                //<4> Read the response
                return promise::async_read(session->socket_, session->buffer_, session->res_);
            })
        .then(
            [=](size_t bytes_transferred)
            {
                boost::ignore_unused(bytes_transferred);
                //<5> Write the message to standard out
                std::cout << session->res_ << std::endl;
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
                session->socket_.shutdown(tcp::socket::shutdown_both, err);
            });
}

promise::Promise getRequest(std::shared_ptr<Session> session, std::string const &host, std::string const &port,
                            std::string const &target, int version)
{
    // Set up an HTTP GET request message
    session->req_ = {};
    session->req_.version(version);
    session->req_.method(http::verb::get);
    session->req_.target(target);
    session->req_.set(http::field::host, host);
    session->req_.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
    return sendRequest(session, host, port);
}

promise::Promise Session::readComic(int id)
{
    return getRequest(shared_from_this(), m_server, "8000", "/comic/" + std::to_string(id), 10)
        .then(
            [=]()
            {
                m_db.resize(id);
                m_db[id] = Comics::fromJson(beast::buffers_to_string(buffer_.cdata()));
            });
}

promise::Promise putRequest(std::shared_ptr<Session> session, std::string const &host, std::string const &port,
                            std::string const &target, int version, const std::string &body)
{
    // Set up an HTTP PUT request message
    session->req_ = {};
    session->req_.version(version);
    session->req_.method(http::verb::put);
    session->req_.target(target);
    session->req_.set(http::field::host, host);
    session->req_.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
    session->req_.body() = body;
    return sendRequest(session, host, port);
}

promise::Promise Session::updateComic(int id)
{
    return putRequest(shared_from_this(), m_server, "8000", "/comic/" + std::to_string(id), 10, toJson(m_db[id]));
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

    // establish http session with server
    session->readComic(0)
        .then(
            [=]()
            {
                Comics::Comic comic = session->m_db[0];
                comic.letters = Comics::findPerson("Brad Templeton");
                updateComic(session->m_db, 0, comic);
            })
        .then(session->updateComic(0))
        .then(
            [=]() {
                std::cout << "Updated comic 0 response: " << beast::buffers_to_string(session->buffer_.cdata()) << '\n';
            });

    ioc.run();

    return EXIT_SUCCESS;
}

} // namespace comicsClient

int main(int argc, char *argv[])
{
    return comicsClient::run(argc, argv);
}
