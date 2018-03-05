/*	$Id: iterators.h 771 2007-05-31 09:42:06Z stuart $
 *	Stuart Pook, Genescope, 2006
*/
#include "mekano.h"
#include <iterator>
#include <utils/eyedb.h>

MakeIterator(Supercontig, Region, regions, const)
MakeIterator(Supercontig, Region, regions,)
MakeIterator(LinkCounts, LinkCount, link_counts, const)
MakeIterator(LinkCounts, LinkCount, link_counts,)
MakeIterator(Contig, ReadPosition, reads, const)
MakeIterator(Contig, ReadPosition, reads,)
MakeIterator(Metacontig, Cut, cuts, const)
MakeIterator(Metacontig, Cut, cuts,)
MakeIterator(Stage, Group, groups, const)
MakeIterator(Stage, Group, groups,)

#undef MakeIterator
