import { error } from '@sveltejs/kit';

/** @type {import('./$types').RequestHandler} */
export async function POST({ request }) {
	let data = await request.json();

	if (!Object.prototype.hasOwnProperty.call(data, 'emptyWeightGramms') || !Object.prototype.hasOwnProperty.call(data, 'fullWeightGramms')) {
		throw error(422, JSON.stringify({ message: 'Invalid data' }));
	}

	let responseBody = { message: 'New bottle weight set' };
	return new Response(JSON.stringify(responseBody), { status: 200 });
}

/** @type {import('./$types').RequestHandler} */
export async function GET() {
	let responseBody = {
		emptyWeightGramms: Math.floor(Math.random() * 10000),
		fullWeightGramms: Math.floor(Math.random() * 10000)
	};
	return new Response(JSON.stringify(responseBody), { status: 200 });
}
