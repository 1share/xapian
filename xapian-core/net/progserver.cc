/* progserver.cc: class for fork()-based server.
 *
 * ----START-LICENCE----
 * Copyright 1999,2000 Dialog Corporation
 * 
 * This program is free software; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License as 
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 * -----END-LICENCE-----
 */

#include "progserver.h"
#include "database.h"
#include "stats.h"
#include "localmatch.h"
#include "netutils.h"
#include "progcommon.h"
#include <memory>

/// The ProgServer constructor, taking two filedescriptors and a database.
ProgServer::ProgServer(auto_ptr<IRDatabase> db_,
		       int readfd_,
		       int writefd_)
	: db(db_), readfd(readfd_), writefd(writefd_)
{
}

/// The ProgServer destructor
ProgServer::~ProgServer()
{
}

void
ProgServer::run()
{
    NetworkStatsGatherer statgath;

    LocalMatch singlematch(db.get());
    singlematch.link_to_multi(&statgath);

    while (1) {
	string message;
	getline(cin, message);
	vector<string> words;

	split_words(message, words);

	if (words.empty()) {
	    break;
	};
	if (words[0] == "GETDOCCOUNT") {
	    cout << db->get_doccount() << endl;
	    cout.flush();
	} else if (words.size() == 2 && words[0] == "SETWEIGHT") {
	    //cerr << "responding to SETWEIGHT" << endl;
	    IRWeight::weight_type wt_type = 
		    static_cast<IRWeight::weight_type>(
		    atol(words[1].c_str()));
	    singlematch.set_weighting(wt_type);
	    cout << "OK" << endl;
	    cout.flush();
	} else if (words[0] == "SETQUERY") {
	    OmQueryInternal temp =
		    query_from_string(message.substr(9,
						     message.npos));
	    singlematch.set_query(&temp);
	    //cerr << "CLIENT QUERY: " << temp.serialise() << endl;
	    cout << "OK" << endl;
	    cout.flush();
	} else if (words[0] == "GETSTATS") {
	    // FIXME: we're not using the RSet stats yet.
	    singlematch.prepare_match();

	    cout << stats_to_string(*statgath.get_stats()) << endl;
	    cout.flush();
	} else if (words[0] == "SETSTATS") {

	    string global_stats;
	    if (words.size() < 2) {
		cout << "ERROR" << endl;
		cout.flush();
	    } else {
		// FIXME: this is a silly way of doing it
		string global_stats = words[1];
		for (vector<string>::size_type i=2; i<words.size(); ++i) {
		    global_stats = global_stats + " " + words[i];
		}
		statgath.set_global_stats(string_to_stats(global_stats));
		cout << "OK" << endl;
		cout.flush();
	    }
	} else if (words[0] == "GET_MSET") {
	    //cerr << "GET_MSET: " << words.size() << " words" << endl;
	    if (words.size() != 3) {
		cout << "ERROR" << endl;
		cout.flush();
	    } else {
		om_doccount first = atoi(words[1].c_str());
		om_doccount maxitems = atoi(words[2].c_str());

		vector<OmMSetItem> mset;
		om_doccount mbound;
		om_weight greatest_wt;

		cerr << "About to get_mset(" << first
			<< ", " << maxitems << "..." << endl;

		singlematch.get_mset(first,
				   maxitems,
				   mset,
				   msetcmp_forward,
				   &mbound,
				   &greatest_wt,
				   0);

		//cerr << "done get_mset..." << endl;

		cout << mset.size() << endl;
		cout.flush();

		//cerr << "sent size..." << endl;

		for (vector<OmMSetItem>::iterator i=mset.begin();
		     i != mset.end();
		     ++i) {
		    cout << i->wt << " " << i->did << endl;
		    cout.flush();

		    cerr << "MSETITEM: " << i->wt << " " << i->did << endl;
		}
		//cerr << "sent items..." << endl;

		cout << "OK" << endl;
		cout.flush();

		//cerr << "sent OK..." << endl;
	    }
	} else if (words[0] == "GETMAXWEIGHT") {
	    cout << singlematch.get_max_weight() << endl;
	    cout.flush();
	} else {
	    cout << "ERROR" << endl;
	    cout.flush();
	}
    }
}
