import { HttpClient } from '@angular/common/http';
import { Injectable } from '@angular/core';
import { Observable } from 'rxjs';
import { map } from 'rxjs/operators';

interface GithubRelease {
  id: number;
  tag_name: string;
  name: string;
  prerelease: boolean;
  draft: boolean;
  assets: { name: string; browser_download_url: string }[];
}

@Injectable({ providedIn: 'root' })
export class GithubUpdateService {
  constructor(private httpClient: HttpClient) { }

  public getReleases(): Observable<GithubRelease[]> {
    return this.httpClient.get<GithubRelease[]>(
      'https://api.github.com/repos/WillItMod/5tratumFW/releases'
    ).pipe(
      // BETA prereleases are the current update channel. Only show releases
      // that provide both OTA images; incomplete packages are not update pairs.
      map(releases => releases.filter(release => !release.draft &&
        ['esp-miner.bin', 'www.bin'].every(name => release.assets?.some(asset => asset.name === name))))
    );
  }
}
