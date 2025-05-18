#ifndef WORKER_H
#define WORKER_H

#include "schema.pb.h"
#include "ClientSocket.h"
#include "CityGraph.h"
#include "Debug.h"

Grid grid = Grid();
CityGraph cityGraph = CityGraph(grid);


struct Worker {
    google::protobuf::Arena arena;
    uint32_t currentGen = 1;
    DijkstraContext dijkstraContext;

    void processRequest(ClientMessage& request) { 
        Request* message = google::protobuf::Arena::Create<Request>(&arena);
        Response* resp = google::protobuf::Arena::CreateMessage<Response>(&arena);
        
        deserializeProtobuf(request, message);
        switch (message->msg_case()) {
            case Request::kWalk: {
                processWalk(message->walk(), resp);
                break;
            }
            
            case Request::kOneToOne: {
                processO2o(message->onetoone(), resp);
                break;
            }

            case Request::kOneToAll: {
                processO2a(message->onetoall(), resp);
                break;
            }

            case Request::kReset: {
                processReset(resp);
                break;
            }

            case Request::MSG_NOT_SET: {
                default:
                    processInvalid(resp);
                    break;
            }
        }

        request.response = serializeProtobuf(*resp);
    }

    Request* deserializeProtobuf(ClientMessage& request, Request* message) {
        const auto& buffer = request.request;

        if (!message->ParseFromArray(buffer.data(), buffer.size())) {
            err(EXIT_FAILURE, "Failed to parse protobuf message");
            return {};
        }

        return message;
    }

    std::vector<uint8_t> serializeProtobuf(const Response& message) {
        const size_t size = message.ByteSizeLong();
        std::vector<uint8_t> buffer(4 + size);

        uint32_t len_net = htonl(static_cast<uint32_t>(size));
        std::memcpy(buffer.data(), &len_net, 4);

        if (!message.SerializeToArray(buffer.data() + 4, static_cast<int>(size))) {
            err(EXIT_FAILURE, "Failed to serialize response message");
        }

        return buffer;
    }

    void processWalk(const Walk& walk, Response* resp) {
        // LOG_DEBUG(walk.DebugString());
        uint32_t nodePrev = grid.addPoint(walk.locations(0).x(), walk.locations(0).y());
        for (int j = 1; j < walk.locations_size(); ++j) {
            uint32_t nodeNext = grid.addPoint(walk.locations(j).x(), walk.locations(j).y());
            cityGraph.addPath(nodePrev, nodeNext, walk.lengths(j-1));
            nodePrev = nodeNext;
        }
        resp->set_status(Response::OK);
    }

    void processO2o(const OneToOne& o2o, Response* resp) {
        LOG_DEBUG(o2o.DebugString());
        uint64_t result = cityGraph.o2o(
            dijkstraContext,
            grid.addPoint(o2o.origin().x(), o2o.origin().y()),
            grid.addPoint(o2o.destination().x(), o2o.destination().y())
        );
        LOG_DEBUG(result);
        resp->set_status(Response::OK);
        resp->set_shortest_path_length(result);
    }

    void processO2a(const OneToAll& o2a, Response* resp) {
        LOG_DEBUG(o2a.DebugString());
        uint64_t result = cityGraph.o2a(
            dijkstraContext,
            grid.addPoint(o2a.origin().x(), o2a.origin().y())
        );
        LOG_DEBUG(result);
        resp->set_status(Response::OK);
        resp->set_total_length(result);
    }

    void processReset(Response* resp) {
        LOG_DEBUG("Received Reset request");
        // cityGraph.reset();
        resp->set_status(Response::OK);
    }

    void processInvalid(Response* resp) {
        LOG_DEBUG("No message type set in Request");
        resp->set_status(Response::ERROR);
    }
};

#endif // WORKER_H