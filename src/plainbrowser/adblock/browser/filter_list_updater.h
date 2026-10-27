// Copyright Plain Browser contributors. MPL-2.0.
#ifndef PLAINBROWSER_ADBLOCK_BROWSER_FILTER_LIST_UPDATER_H_
#define PLAINBROWSER_ADBLOCK_BROWSER_FILTER_LIST_UPDATER_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/timer/timer.h"
#include "components/prefs/pref_change_registrar.h"
#include "plainbrowser/adblock/browser/filter_lists.h"
#include "url/gurl.h"

class PrefRegistrySimple;
class PrefService;

namespace network {
class SharedURLLoaderFactory;
class SimpleURLLoader;
}  // namespace network

namespace plainbrowser::adblock {

// Keeps EasyList, EasyPrivacy and EasyList Polish current between browser
// releases. Once a day (pref prefs::kListUpdatesLastCheck) it fetches the
// manifest from lists.plain-browser.app (filter_lists::kDefaultServerUrl),
// downloads the lists whose hash changed, verifies size and SHA-256, stores
// them in the user data directory and has AdBlockService build a new engine
// from them; lists not downloaded stay the compiled-in ones. At start-up the
// stored lists are loaded right away. Requests carry no cookies and only the
// X-Client header the Safe Browsing proxy also gets.
//
// Local State pref prefs::kListUpdatesEnabled (on by default, toggled on
// chrome://settings/privacy) turns it off: no requests, the stored lists are
// deleted and the engine goes back to the compiled-in lists.
//
// UI thread. Started once from ChromeBrowserMainParts::PostBrowserStart
// (patches/30-adblock/0006); lives until the process exits.
class FilterListUpdater {
 public:
  // Called from RegisterLocalState (patches/30-adblock/0006).
  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

  static void Start(PrefService* local_state,
                    scoped_refptr<network::SharedURLLoaderFactory> factory,
                    const base::FilePath& user_data_dir);

  FilterListUpdater(const FilterListUpdater&) = delete;
  FilterListUpdater& operator=(const FilterListUpdater&) = delete;

 private:
  FilterListUpdater(PrefService* local_state,
                    scoped_refptr<network::SharedURLLoaderFactory> factory,
                    const base::FilePath& user_data_dir);
  ~FilterListUpdater();

  bool Enabled() const;
  void OnEnabledChanged();
  void LoadStore();
  void OnStoreRead(filter_lists::Store store);

  void ScheduleCheck(base::TimeDelta delay);
  void Check();
  void Fetch(const GURL& url,
             size_t max_bytes,
             base::OnceCallback<void(std::unique_ptr<std::string>)> done);
  void OnManifest(std::unique_ptr<std::string> body);
  void FetchNextList();
  void OnList(std::string name, std::unique_ptr<std::string> body);
  void FinishCheck(bool success);

  const raw_ptr<PrefService> local_state_;
  const scoped_refptr<network::SharedURLLoaderFactory> factory_;
  const base::FilePath store_dir_;
  const GURL server_;
  const scoped_refptr<base::SequencedTaskRunner> file_runner_;
  PrefChangeRegistrar registrar_;
  base::OneShotTimer timer_;

  // The verified manifest of the store on disk.
  filter_lists::Manifest stored_;
  // The engine uses downloaded lists (so disabling has something to undo).
  bool using_downloaded_ = false;

  // The check in progress.
  std::unique_ptr<network::SimpleURLLoader> loader_;
  filter_lists::Manifest wanted_;
  std::vector<std::string> pending_;
  std::vector<std::pair<std::string, std::string>> fetched_;

  base::WeakPtrFactory<FilterListUpdater> weak_factory_{this};
};

}  // namespace plainbrowser::adblock

#endif  // PLAINBROWSER_ADBLOCK_BROWSER_FILTER_LIST_UPDATER_H_
