import { TestBed } from '@angular/core/testing';
import { provideHttpClient } from '@angular/common/http';
import { HttpTestingController, provideHttpClientTesting } from '@angular/common/http/testing';
import { GithubUpdateService } from './github-update.service';

describe('GithubUpdateService', () => {
  let service: GithubUpdateService;
  let http: HttpTestingController;
  beforeEach(() => {
    TestBed.configureTestingModule({ providers: [provideHttpClient(), provideHttpClientTesting()] });
    service = TestBed.inject(GithubUpdateService);
    http = TestBed.inject(HttpTestingController);
  });
  afterEach(() => http.verify());

  it('keeps complete BETA pairs from the WillItMod firmware repository', () => {
    const beta = { id: 1, name: 'BETA 1', tag_name: 'v0.1.0-beta.1', prerelease: true, draft: false,
      assets: [{ name: 'esp-miner.bin' }, { name: 'www.bin' }] };
    service.getReleases().subscribe(releases => expect(releases.map(release => release.id)).toEqual([1]));
    http.expectOne('https://api.github.com/repos/WillItMod/5tratumFW/releases').flush([beta]);
  });

  it('omits drafts and incomplete image pairs', () => {
    service.getReleases().subscribe(releases => expect(releases).toEqual([]));
    http.expectOne('https://api.github.com/repos/WillItMod/5tratumFW/releases').flush([
      { id: 2, draft: true, assets: [{ name: 'esp-miner.bin' }, { name: 'www.bin' }] },
      { id: 3, draft: false, assets: [{ name: 'www.bin' }] },
      { id: 4, draft: false }
    ]);
  });
});
