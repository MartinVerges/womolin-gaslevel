<script>
	import Fa from 'svelte-fa/src/fa.svelte';
	import { faRv } from '@fortawesome/pro-solid-svg-icons/faRv';
	import { faHouseChimney } from '@fortawesome/pro-solid-svg-icons/faHouseChimney';
	import { faWifi } from '@fortawesome/pro-solid-svg-icons/faWifi';
	import { faGear } from '@fortawesome/pro-solid-svg-icons/faGear';
	import { faScaleBalanced } from '@fortawesome/pro-solid-svg-icons/faScaleBalanced';
	import { page } from '$app/stores';
	let path;
	$: path = $page.url.pathname;
</script>

<header>
	<div class="px-3 py-2 bg-dark text-white">
		<div class="container">
			<div class="d-flex flex-wrap align-items-center justify-content-center justify-content-lg-start">
				<a href="/" class="d-flex align-items-center my-2 my-lg-0 me-lg-auto text-white text-decoration-none"><Fa id="mainicon" icon={faRv} size="3x" /></a>

				<ul class="nav col-12 col-lg-auto my-2 justify-content-center my-md-0 text-small">
					<li>
						<a href="/" class="nav-link {path === '/' ? 'text-secondary' : 'text-white'}">
							<Fa icon={faHouseChimney} size="lg" class="bi d-block mx-auto mb-1" />Home
						</a>
					</li>
					<li>
						<a href="/calibrate/" class="nav-link {path === '/calibrate/' ? 'text-secondary' : 'text-white'}">
							<Fa icon={faScaleBalanced} size="lg" class="bi d-block mx-auto mb-1" />Calibrate
						</a>
					</li>
					<li>
						<a href="/wifi/" class="nav-link {path === '/wifi/' ? 'text-secondary' : 'text-white'}">
							<Fa icon={faWifi} size="lg" class="bi d-block mx-auto mb-1" />Wifi
						</a>
					</li>
					<li>
						<a href="/settings/" class="nav-link {path === '/settings/' ? 'text-secondary' : 'text-white'}">
							<Fa icon={faGear} size="lg" class="bi d-block mx-auto mb-1" />Settings
						</a>
					</li>
				</ul>
			</div>
		</div>
	</div>
</header>
