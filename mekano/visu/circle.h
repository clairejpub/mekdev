/*	Stuart Pook, Genoscope 2007 (Indent ON)
 *	$Id: circle.h 743 2007-05-16 10:26:13Z stuart $
 */
#include "utils.h"
#include "bcm.h"
#include "visu.h"
#include "graphics.h"

void
open_circle
(
	scaf_list_t const & slist,
	BankColourMap * bcm,
	ObjectHandle<mekano::Parameters> const & parameters,
	std::string const & title,
	std::string const & text
);
