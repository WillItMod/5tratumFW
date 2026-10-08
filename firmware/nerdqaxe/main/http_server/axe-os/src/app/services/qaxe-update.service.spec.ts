import { TestBed } from '@angular/core/testing';
import { provideHttpClient, withInterceptors } from '@angular/common/http';
import { HttpTestingController, provideHttpClientTesting } from '@angular/common/http/testing';
import { QaxeUpdateCheck, QaxeUpdateService } from './qaxe-update.service';

const REPOSITORY = 'https://github.com/WillItMod/5tratumFW';
const CATALOG = 'https://api.github.com/repos/WillItMod/5tratumFW/releases?per_page=100&page=';

function release(tag = 'qaxe-v0.1.0-beta.2'): any {
  return { tag_name: tag, draft: false, prerelease: tag.includes('-beta'), html_url: `${REPOSITORY}/releases/tag/${tag}`,
    assets: ['esp-miner-NerdQAxe++.bin', 'www.bin'].map(name => ({ name, size: 3000000,
      browser_download_url: `${REPOSITORY}/releases/download/${tag}/${encodeURIComponent(name)}` })) };
}

describe('QAxe public release checks', () => {
  let service: QaxeUpdateService;
  let http: HttpTestingController;

  beforeEach(() => {
    TestBed.configureTestingModule({ providers: [
      provideHttpClient(withInterceptors([(request, next) => next(request.clone({
        setHeaders: { 'X-OTP-Session': 'miner-session-fixture', Authorization: 'miner-auth-fixture' },
      }))])),
      provideHttpClientTesting(),
    ] });
    service = TestBed.inject(QaxeUpdateService);
    http = TestBed.inject(HttpTestingController);
  });
  afterEach(() => http.verify());

  function check(rows: any[], current = '5tratumFW-qa-0.1.0-beta.1'): QaxeUpdateCheck {
    let result!: QaxeUpdateCheck;
    service.check('NerdQAxe++', current).subscribe(value => result = value);
    const request = http.expectOne(CATALOG + '1');
    expect(request.request.method).toBe('GET');
    expect(request.request.headers.has('X-OTP-Session')).toBeFalse();
    expect(request.request.headers.has('Authorization')).toBeFalse();
    expect(request.request.withCredentials).toBeFalse();
    request.flush(rows);
    return result;
  }

  it('includes QAxe prereleases and keeps both download links in the same release without miner auth', () => {
    const result = check([release()]);
    expect(result.status).toBe('available');
    expect(result.release?.version).toBe('5tratumFW-qa-0.1.0-beta.2');
    expect(result.release?.prerelease).toBeTrue();
    expect(result.release?.appUrl).toContain('/qaxe-v0.1.0-beta.2/esp-miner-NerdQAxe%2B%2B.bin');
    expect(result.release?.webUrl).toContain('/qaxe-v0.1.0-beta.2/www.bin');
    http.expectNone(request => request.url.endsWith('/latest') || request.method !== 'GET');
  });

  it('does not offer Gamma, stock Nerd, another model or incomplete QAxe release pairs', () => {
    const gamma = release('v0.9.99'), upstream = release('v1.1.0'), wrongModel = release(), incomplete = release();
    wrongModel.assets[0].name = 'esp-miner-NerdOctAxe.bin';
    incomplete.assets = incomplete.assets.slice(1);
    expect(check([gamma, upstream, wrongModel, incomplete])).toEqual({ status: 'no-release', release: null });
  });

  it('never assembles the application and WWW from different releases', () => {
    const first = release('qaxe-v0.1.0-beta.1'), second = release();
    first.assets = first.assets.slice(0, 1); second.assets = second.assets.slice(1);
    expect(check([first, second]).status).toBe('no-release');
  });

  it('rejects drafts, duplicate assets and asset URLs outside the exact release', () => {
    const draft = release(), duplicate = release(), foreign = release(), wrongTag = release();
    draft.draft = true;
    duplicate.assets.push({ ...duplicate.assets[0] });
    foreign.assets[0].browser_download_url = 'https://example.com/esp-miner-NerdQAxe++.bin';
    wrongTag.assets[1].browser_download_url = `${REPOSITORY}/releases/download/qaxe-v0.1.0-beta.1/www.bin`;
    expect(check([draft, duplicate, foreign, wrongTag]).status).toBe('no-release');
  });

  it('rejects forged release links, credentials, queries, malformed URLs and empty artifacts', () => {
    const invalid = [release(), release(), release(), release(), release()];
    invalid[0].html_url = 'https://github.com/Other/5tratumFW/releases/tag/qaxe-v0.1.0-beta.2';
    invalid[1].assets[0].browser_download_url = 'https://user@github.com/WillItMod/5tratumFW/releases/download/qaxe-v0.1.0-beta.2/esp-miner-NerdQAxe%2B%2B.bin';
    invalid[2].assets[0].browser_download_url += '?download=1';
    invalid[3].assets[0].browser_download_url = 'not a URL';
    invalid[4].assets[0].size = 0;
    expect(check(invalid).status).toBe('no-release');
  });

  it('sorts beta versions numerically instead of taking the first catalog row', () => {
    expect(check([release(), release('qaxe-v0.1.0-beta.10')]).release?.version)
      .toBe('5tratumFW-qa-0.1.0-beta.10');
  });

  it('does not treat malformed QAxe tags as semantic release versions', () => {
    expect(check([release('qaxe-v0.1.0-beta.02'), release('qaxe-v0.1.0-beta'),
      release('qaxe-v0.1.9007199254740992')]).status).toBe('no-release');
  });

  it('recognizes an equal application version and a newer installed build', () => {
    expect(check([release()], '5tratumFW-qa-0.1.0-beta.2').status).toBe('up-to-date');
    expect(check([release()], '5tratumFW-qa-0.1.0-beta.3').status).toBe('newer-build');
  });

  it('orders a stable version after its prereleases and offers a release for upstream firmware', () => {
    expect(check([release('qaxe-v0.1.0'), release('qaxe-v0.1.0-beta.10')]).release?.version)
      .toBe('5tratumFW-qa-0.1.0');
    expect(check([release()], 'v1.0.37.3-LTS').status).toBe('available');
  });

  it('finds QAxe releases beyond a full first page of other firmware', () => {
    let result!: QaxeUpdateCheck;
    service.check('NerdQAxe++', '5tratumFW-qa-0.1.0-beta.1').subscribe(value => result = value);
    http.expectOne(CATALOG + '1').flush(Array.from({ length: 100 }, () => release('v0.9.99')));
    http.expectOne(CATALOG + '2').flush([release()]);
    expect(result.status).toBe('available');
  });

  it('does not contact GitHub for another model', () => {
    let result!: QaxeUpdateCheck;
    service.check('NerdQAxe+', '5tratumFW-test').subscribe(value => result = value);
    expect(result.status).toBe('unsupported');
    http.expectNone(request => request.url.startsWith('https://api.github.com'));
  });

  it('reports catalog errors instead of treating them as no available release', () => {
    let error: any;
    service.check('NerdQAxe++', '5tratumFW-qa-0.1.0-beta.1').subscribe({ error: value => error = value });
    http.expectOne(CATALOG + '1').flush('Unavailable', { status: 503, statusText: 'Service Unavailable' });
    expect(error.status).toBe(503);
    error = undefined;
    service.check('NerdQAxe++', '5tratumFW-qa-0.1.0-beta.1').subscribe({ error: value => error = value });
    http.expectOne(CATALOG + '1').flush({ message: 'not a release list' });
    expect(error.message).toBe('Invalid release catalog.');
  });
});
