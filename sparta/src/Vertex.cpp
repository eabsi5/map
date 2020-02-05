// <Vertex> -*- C++ -*-


#include <cstdint>
#include <map>
#include <ostream>
#include <string>
#include <utility>

#include "sparta/kernel/DAG.hpp"
#include "sparta/kernel/Scheduler.hpp"
#include "sparta/kernel/Vertex.hpp"
#include "sparta/kernel/EdgeFactory.hpp"
#include "sparta/utils/SpartaAssert.hpp"
#include "sparta/events/Scheduleable.hpp"

namespace sparta
{

    // Static global vertex and edge ID's
    uint32_t Vertex::global_id_  = 0;
    uint32_t Edge::global_id_ = 0;

    bool Vertex::link(EdgeFactory& efact, Vertex * dest, const std::string& label)
    {
        if(dest == this) return false;

        if (outbound_edge_map_.find(dest) != outbound_edge_map_.end()) {
            // Edge already present -- not necessary to add it again
            return false;
        } else {
            const Edge* new_edge = efact.newFactoryEdge(this, dest, label);
            outbound_edge_map_[dest] = new_edge;
            outbound_edge_list_.push_back(dest);
            ++(dest->num_inbound_edges_);
        }
        return true;
    }

    bool Vertex::unlink(EdgeFactory& efact, Vertex * w)
    {
        if(w == this) return false;

        auto ei = outbound_edge_map_.find(w);
        if (ei == outbound_edge_map_.end()) {
            // Edge not present -- just ignore
            return false;
        } else {
            for (auto el = outbound_edge_list_.begin(); el != outbound_edge_list_.end(); ++el) {
                if (*el == ei->first) {
                    outbound_edge_list_.erase(el);
                }
            }
            outbound_edge_map_.erase(ei);
            sparta_assert(w->num_inbound_edges_ > 0);
            --(w->num_inbound_edges_);
            efact.removeEdge(ei->second);
        }
        return true;
    }

    void Vertex::assignConsumerGroupIDs(VertexList &zlist)
    {

        uint32_t gid = getGroupID();

        for(auto &ei : sorting_edges_)
        {
            Vertex * outbound = ei.first;
            // The outbound edge better have a count of edges by at
            // LEAST one -- it has to include this link!
            sparta_assert(outbound->sorted_num_inbound_edges_ > 0);
            --(outbound->sorted_num_inbound_edges_);

            // If the destination's group ID is at or less than this
            // source's ID, bump it -- there's a dependency
            if (outbound->getGroupID() <= gid) {
                outbound->setGroupID(gid + 1);
            }

            // If there are no other inputs to this Vertex, it's now
            // on the zlist to recursively set it's destination group
            // IDs.
            if (outbound->sorted_num_inbound_edges_ == 0) {
                zlist.push_back(outbound);
            }

        }
    }

    /**
     * Detect whether the DAG has at least one cycle somewhere
     * At the completion of detectCycle(), the DAG vertices will
     * be marked:
     *     WHITE if they have NOT be visited
     *     GRAY if they have been visited, and ARE part of a cycle
     *     BLACK if they have been visited, but are NOT part of a cycle
     */
    bool Vertex::detectCycle()
    {
        // Mark that we've visited this (current) vertex
        marker_ = CycleMarker::GRAY;

        // Loop through this vertex's outbound edges...
        for (auto& ei : outbound_edge_map_) {
            Vertex *w = ei.first;

            switch (w->marker_) {
            // w has not been visited, recurse down this edge
            case CycleMarker::WHITE:
                if (w->detectCycle()) {
                    return true;
                }
                break;

            // w has already been visited, so we have a cycle
            case CycleMarker::GRAY:
                return true;

            // w is "finished" (i.e. BLACK), nothing to see here
            default:
                break; // Do nothing
            }
        }

        // Done with checking the edge paths from this vertex
        marker_ = CycleMarker::BLACK;
        return false;
    }

    /**
     * Return the set of vertices that are part of a DAG cycle
     *
     * If a cycle is found, return true and provide set of vertices
     * in the cycle.
     *
     * If a cycle is NOT found, return false, and leave cycle_set
     * untouched.
     *
     * At the completion of findCycle(), the DAG vertices will
     * be marked:
     *     WHITE if they have NOT be visited
     *     GRAY if they have been visited, and ARE part of a cycle
     *     BLACK if they have been visited, but are NOT part of a cycle
     *
     * NOTE: This routine does the same white/gray/black traversal
     * as detectCycle_. We COULD just return all the gray vertices
     * after calling detectCycle_(). Is it computationally cheaper to
     * repeat the traversal, or just scan through all the (many!) allocated
     * vertices?
     */
    bool Vertex::findCycle(VertexList& cycle_set)
    {
        // Mark that we've visited this (current) vertex
        marker_ = CycleMarker::GRAY;

        // Loop through this vertex's outbound edges...
        for (auto& ei : outbound_edge_map_) {
            Vertex *w = ei.first;

            switch (w->marker_) {
            // w has not been visited, recurse down this edge
            case CycleMarker::WHITE:
                if (w->findCycle(cycle_set)) {
                    cycle_set.push_front(w);
                    return true;
                }
                break;

            // w has already been visited, so we have a cycle
            case CycleMarker::GRAY:
                cycle_set.push_front(w);
                return true;

            // w is "finished" (i.e. BLACK), nothing to see here
            default:
                break; // Do nothing
            }
        }

        // Done with checking the edge paths from this vertex
        marker_ = CycleMarker::BLACK;
        return false;
    }

    void Vertex::precedes(Scheduleable & s, const std::string & label) {
        sparta_assert(my_scheduler_);
        DAG * dag = my_scheduler_->getDAG();
        sparta_assert(dag->isFinalized() == false,
                    "You cannot set precedence during a running simulation (i.e., the DAG is finalized)");
        dag->link(this, s.getVertex(), label);
    }

    // Dump this vertex to the provided CSV ostream
    void Vertex::dumpToCSV(std::ostream& os, bool dump_header) const
    {
        std::ios_base::fmtflags os_state(os.flags());

        if (dump_header) {
            os << "vertex_id,type,group_id,marker,label" << std::endl;
        }

        os << std::dec << id_
           << "," << (isGOP() ? "G" : "V")
           << "," << getGroupID()
           << "," << (marker_ == CycleMarker::WHITE ? "white" : (marker_ == CycleMarker::GRAY ? "gray" : "black"))
           << ",\"" << getLabel() << "\""
           << std::endl;

        os.flags(os_state);
    }

    void Vertex::print(std::ostream& os) const
    {
        std::ios_base::fmtflags os_state(os.flags());
        os << std::string(*this) << std::endl;
        for (const auto & ei : outbound_edge_map_) {
            //os << "\t-> " << std::string(*(ei.first)) << ", " << std::string(*(ei.second)) << std::endl;
            os << "\t-> " << std::string(*(ei.first)) << std::endl;
        }
        os << std::endl;
        os.flags(os_state);
    }

    void Vertex::printFiltered(std::ostream &os, CycleMarker matchingMarker) const
    {
        std::ios_base::fmtflags os_state(os.flags());
        os << std::string(*this) << std::endl;
        for (const auto & ei : outbound_edge_map_) {
            //os << "\t-> " << std::string(*(ei.first)) << ", " << std::string(*(ei.second)) << std::endl;
            if (ei.first->marker_ == matchingMarker) {
                os << "\t-> " << std::string(*(ei.first)) << std::endl;
            }
        }
        os << std::endl;
        os.flags(os_state);
    }
}
