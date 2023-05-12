// ============================================================================================================================ 132
//
//  proxy.cc
//
//      A simple IPv4 TCP proxy
//
//  COLUMNS 132 TABSTOP 4 SPACE-FILL
//
// ============================================================================================================================ 132

/* ============================================================================


Copyright 1998-2022 Jack Bates

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the “Software”), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

============================================================================ */

#include "AlarmDebugLog.h"
#include "ThreadMinimal.h"
#include "Socket.h"

using namespace libthrocket;

const int64_t accept_timeout     =   5 * 1000 * 1000;   // accept() polling loop
const int64_t recv_timeout       =   5 * 1000 * 1000;   // recv() timeout, not really used due to non-blocking I/O
const int64_t send_timeout       =  30 * 1000 * 1000;   // send() hard time limit to transmit buffer
const int64_t wait_timeout       =   5 * 1000 * 1000;   // wait() polling loop on read of socket
const size_t  transfer_buf_bytes =   1 * 1024 * 1024;   // maximum block to transfer at a time

// these are intended to be created as bidirectional pairs
// each peer only closes the socket that it's reading from
class TransferThread : public Thread
{
public:
                        TransferThread(TCPSocket * s_read, TCPSocket * s_write)   :
                            m_s_read(s_read), m_s_write(s_write)
                        {
                        }
    virtual             ~TransferThread()
                        {
                            if (m_s_read != nullptr)
                                m_s_read->Close();
                        }

    static bool         GlobalShutdownRequested()
                        { return m_bGlobalShutdownRequested; }
    static void         GlobalShutdownRequested(bool b)
                        {
                            if (!m_bGlobalShutdownRequested && b)
                                m_bGlobalShutdownRequested = b;
                        }

    virtual void        Run();

private:
    TCPSocket   * m_s_read;
    TCPSocket   * m_s_write;
    uint8_t       m_transfer_buf[transfer_buf_bytes];

    static bool   m_bGlobalShutdownRequested;
};

bool TransferThread::m_bGlobalShutdownRequested = false;

void TransferThread::Run()
{
    try
    {
        while (!GlobalShutdownRequested())
        {
            try
            {
                m_s_read->Wait(true/*bWantRead*/, false/*bWantWrite*/, wait_timeout);
            }
            catch (const SocketTimeoutException & e)
            { continue; }

            size_t got = m_s_read->Recv(m_transfer_buf, transfer_buf_bytes, true/*bShort*/);
            if (got == 0)
                break;
            m_s_write->Send(m_transfer_buf, got);
        }
    }
    catch (const Exception & e)
    {
        LOGERROR("TransferThread: caught exception:");
        e.LogError(LIBTHROCKET_CAUGHT_BY);
    }
    catch (const std::exception & e)
    {
        LOGERROR("TransferThread: std::exception: '%s', exiting.", e.what());
    }
    catch (...)
    {
        LOGERROR("TransferThread: wild exception, exiting.");
    }
}

void service(const std::string & bind_ip, uint16_t bind_port, const std::string & connect_ip, uint16_t connect_port)
{
    LOGTRACE("startup: %s:%d -> %s:%d", bind_ip.c_str(), bind_port, connect_ip.c_str(), connect_port);

    ThreadMother mother;

    try
    {
        TCPAcceptSocket s_accept(recv_timeout, send_timeout);
        s_accept.Bind(bind_ip, bind_port);
        while (!TransferThread::GlobalShutdownRequested())
        {
            // busy server, this is a bottleneck
            mother.ReapChildren();

            TCPSocket * s_read = nullptr;
            try
            {
                s_read = s_accept.Accept(accept_timeout);
                s_read->SetNonBlocking();
                s_read->NoNagle();
            }
            catch (const SocketTimeoutException & e)
            { continue; }

            TCPSocket * s_write = new TCPSocket(recv_timeout, send_timeout);
            try
            {
                s_write->Connect(connect_ip, connect_port);
                s_write->SetNonBlocking();
                s_write->NoNagle();

                // track and start both threads - one for each direction
                mother.ChildBirth(new TransferThread(s_read, s_write))->go();
                mother.ChildBirth(new TransferThread(s_write, s_read))->go();
            }
            catch (const Exception & e)
            {
                LOGERROR("service: spawn: caught exception, exiting.");
                e.LogError(LIBTHROCKET_CAUGHT_BY);
                break;
            }
            catch (const std::exception & e)
            {
                LOGERROR("service: spawn: std::exception: '%s', exiting.", e.what());
                break;
            }
            catch (...)
            {
                LOGERROR("service: spawn: wild exception, exiting.");
                break;
            }
        }
    }
    catch (const Exception & e)
    {
        LOGERROR("service: top-level: caught exception, exiting.");
        e.LogError(LIBTHROCKET_CAUGHT_BY);
    }
    catch (const std::exception & e)
    {
        LOGERROR("service: top-level: std::exception: '%s', exiting.", e.what());
    }
    catch (...)
    {
        LOGERROR("service: top-level: wild exception, exiting.");
    }

    TransferThread::GlobalShutdownRequested(true);

    while (mother.GetNumChildren())
        mother.ReapChildren();
}

void Usage(const char * msg)
{
    fprintf(stderr, "usage: proxy bind_ip bind_port connect_ip connect_port\n");
    fprintf(stderr, "       %s\n", msg);
    exit(1);
}

int main(int argc, char * argv[])
{
    ADL_syslog_ident = const_cast<char *>("TCP-proxy");
    ADL_debug_level  = ADL_DLVL_NONE; // AlarmDebugLog control - i.e. turn debugging off
    ADL_debug_mask   = ADL_DMSK_NONE; // AlarmDeubgLog control - i.e. turn debugging off

    if (argc != 5)
        Usage("invalid argument count");

    std::string bind_ip         =   argv[1];
    uint16_t    bind_port       =   atoi(argv[2]);
    std::string connect_ip      =   argv[3];
    uint16_t    connect_port    =   atoi(argv[4]);

    if (!InetSocket::ValidIPv4Addr(bind_ip))
        Usage("invalid bind_ip");
    if (!InetSocket::ValidIPv4Port(bind_port))
        Usage("invalid bind_port");
    if (!InetSocket::ValidIPv4Addr(connect_ip))
        Usage("invalid connect_ip");
    if (!InetSocket::ValidIPv4Port(connect_port))
        Usage("invalid connect_port");

    // daemonize: chdir to "/", close stdio
    daemon(0, 0);

    service(bind_ip, bind_port, connect_ip, connect_port);

    return 0;
}
